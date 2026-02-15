#!/usr/bin/env python3
"""
Project Template Manager

Manages templates for the Wizardoz ESP32 project:
- backup: Copy current src/ to templates/<name>/
- select: Copy templates/<name>/ to src/
- list: Show available templates

Usage:
    python manage_template.py backup my-game
    python manage_template.py select voice-jump-game
    python manage_template.py list
    python manage_template.py delete old-template
"""

import argparse
import os
import shutil
import sys
from pathlib import Path
from datetime import datetime

PROJECT_ROOT = Path(__file__).parent.resolve()
SRC_DIR = PROJECT_ROOT / "src"
TEMPLATES_DIR = PROJECT_ROOT / "templates"

EXCLUDE_PATTERNS = [
    "*.pyc",
    "__pycache__",
    ".pio",
    ".vscode",
    "*.tmp",
    ".DS_Store",
    "*.log",
]


def should_exclude(path: Path) -> bool:
    """Check if path should be excluded based on patterns."""
    path_str = str(path)
    for pattern in EXCLUDE_PATTERNS:
        if pattern.startswith("*"):
            if path_str.endswith(pattern[1:]):
                return True
        elif pattern in path_str:
            return True
    return False


def copy_tree(src: Path, dst: Path, exclude_patterns=None):
    """Copy directory tree with exclusion support."""
    if exclude_patterns is None:
        exclude_patterns = EXCLUDE_PATTERNS

    if not src.exists():
        raise FileNotFoundError(f"Source directory not found: {src}")

    dst.mkdir(parents=True, exist_ok=True)

    for item in src.rglob("*"):
        if should_exclude(item):
            continue

        rel_path = item.relative_to(src)
        dst_path = dst / rel_path

        if item.is_dir():
            dst_path.mkdir(parents=True, exist_ok=True)
        else:
            dst_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, dst_path)


def backup_template(name: str, description: str = None):
    """Backup current src/ to templates/<name>/."""
    if not SRC_DIR.exists():
        print(f"Error: Source directory not found: {SRC_DIR}")
        sys.exit(1)

    if not name:
        print("Error: Template name is required")
        sys.exit(1)

    if not name.replace("-", "").replace("_", "").isalnum():
        print(
            f"Error: Template name must be alphanumeric (with '-' or '_' allowed): {name}"
        )
        sys.exit(1)

    template_dir = TEMPLATES_DIR / name

    if template_dir.exists():
        response = input(f"Template '{name}' already exists. Overwrite? (y/N): ")
        if response.lower() != "y":
            print("Backup cancelled.")
            return
        shutil.rmtree(template_dir)

    print(f"Backing up src/ to templates/{name}/...")

    try:
        copy_tree(SRC_DIR, template_dir)

        meta_file = template_dir / ".template_meta"
        timestamp = datetime.now().isoformat()
        meta_content = f"""# Template Metadata
name: {name}
created: {timestamp}
source: src/
"""
        if description:
            meta_content += f"description: {description}\n"

        meta_file.write_text(meta_content)

        file_count = len(
            [
                f
                for f in template_dir.rglob("*")
                if f.is_file() and f.name != ".template_meta"
            ]
        )
        dir_count = len([d for d in template_dir.rglob("*") if d.is_dir()])

        print(f"✓ Backup complete: templates/{name}/")
        print(f"  Files: {file_count}, Directories: {dir_count}")

    except Exception as e:
        print(f"Error during backup: {e}")
        sys.exit(1)


def select_template(name: str, force: bool = False):
    """Copy templates/<name>/ to src/."""
    template_dir = TEMPLATES_DIR / name

    if not template_dir.exists():
        print(f"Error: Template not found: templates/{name}/")
        print(f"Run 'python manage_template.py list' to see available templates.")
        sys.exit(1)

    if SRC_DIR.exists() and any(SRC_DIR.iterdir()):
        if not force:
            response = input(
                f"src/ already has content. Replace with template '{name}'? (y/N): "
            )
            if response.lower() != "y":
                print("Selection cancelled.")
                return

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_name = f"src_backup_{timestamp}"
        backup_dir = PROJECT_ROOT / backup_name
        print(f"Creating backup of current src/: {backup_name}/")
        copy_tree(SRC_DIR, backup_dir)

        shutil.rmtree(SRC_DIR)

    print(f"Selecting template: {name}...")

    try:
        copy_tree(template_dir, SRC_DIR)

        meta_file = SRC_DIR / ".template_meta"
        if meta_file.exists():
            meta_file.unlink()

        file_count = len([f for f in SRC_DIR.rglob("*") if f.is_file()])
        dir_count = len([d for d in SRC_DIR.rglob("*") if d.is_dir()])

        print(f"✓ Template '{name}' selected and copied to src/")
        print(f"  Files: {file_count}, Directories: {dir_count}")

    except Exception as e:
        print(f"Error during selection: {e}")
        sys.exit(1)


def list_templates():
    """List all available templates."""
    if not TEMPLATES_DIR.exists():
        print("No templates directory found.")
        return

    templates = [d for d in TEMPLATES_DIR.iterdir() if d.is_dir()]

    if not templates:
        print("No templates found in templates/")
        return

    print(f"\nAvailable templates ({len(templates)} total):\n")
    print(f"{'Name':<25} {'Files':<8} {'Size':<12} {'Description'}")
    print("-" * 70)

    for template in sorted(templates):
        name = template.name
        files = len([f for f in template.rglob("*") if f.is_file()])

        total_size = sum(f.stat().st_size for f in template.rglob("*") if f.is_file())
        size_str = format_size(total_size)

        desc = ""
        meta_file = template / ".template_meta"
        if meta_file.exists():
            for line in meta_file.read_text().split("\n"):
                if line.startswith("description:"):
                    desc = line.split(":", 1)[1].strip()
                    break

        print(f"{name:<25} {files:<8} {size_str:<12} {desc}")

    print()


def format_size(size_bytes: int) -> str:
    """Format bytes to human readable string."""
    for unit in ["B", "KB", "MB", "GB"]:
        if size_bytes < 1024.0:
            return f"{size_bytes:.1f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.1f} TB"


def delete_template(name: str, force: bool = False):
    """Delete a template."""
    template_dir = TEMPLATES_DIR / name

    if not template_dir.exists():
        print(f"Error: Template not found: templates/{name}/")
        sys.exit(1)

    if not force:
        response = input(f"Are you sure you want to delete template '{name}'? (y/N): ")
        if response.lower() != "y":
            print("Deletion cancelled.")
            return

    try:
        shutil.rmtree(template_dir)
        print(f"✓ Template '{name}' deleted.")
    except Exception as e:
        print(f"Error deleting template: {e}")
        sys.exit(1)


def show_status():
    """Show current project status."""
    print("\nProject Status:")
    print("-" * 40)

    if SRC_DIR.exists():
        files = len([f for f in SRC_DIR.rglob("*") if f.is_file()])
        dirs = len([d for d in SRC_DIR.rglob("*") if d.is_dir()])
        print(f"src/: {files} files, {dirs} directories")

        for template in TEMPLATES_DIR.iterdir():
            if template.is_dir():
                src_main = SRC_DIR / "main.cpp"
                tmpl_main = template / "src" / "main.cpp"
                if src_main.exists() and tmpl_main.exists():
                    if src_main.read_text() == tmpl_main.read_text():
                        print(f"Current template: {template.name}")
                        break
    else:
        print("src/: Not found")

    if TEMPLATES_DIR.exists():
        templates = [d.name for d in TEMPLATES_DIR.iterdir() if d.is_dir()]
        print(f"templates/: {len(templates)} templates available")
    else:
        print("templates/: Not found")

    print()


def main():
    parser = argparse.ArgumentParser(
        description="Manage ESP32 project templates",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s backup my-new-game           # Backup current src/ as template
  %(prog)s backup my-game -d "Platformer with jump mechanics"
  %(prog)s select voice-jump-game       # Restore template to src/
  %(prog)s select voice-jump-game -f    # Force overwrite without prompt
  %(prog)s list                         # Show all templates
  %(prog)s delete old-template          # Remove a template
  %(prog)s status                       # Show current project status
        """,
    )

    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    backup_parser = subparsers.add_parser(
        "backup", help="Backup src/ to templates/<name>/"
    )
    backup_parser.add_argument("name", help="Template name")
    backup_parser.add_argument("-d", "--description", help="Template description")

    select_parser = subparsers.add_parser("select", help="Copy template to src/")
    select_parser.add_argument("name", help="Template name")
    select_parser.add_argument(
        "-f", "--force", action="store_true", help="Force overwrite without prompt"
    )

    subparsers.add_parser("list", help="List available templates")

    delete_parser = subparsers.add_parser("delete", help="Delete a template")
    delete_parser.add_argument("name", help="Template name")
    delete_parser.add_argument(
        "-f", "--force", action="store_true", help="Force delete without prompt"
    )

    subparsers.add_parser("status", help="Show project status")

    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        sys.exit(1)

    if args.command == "backup":
        backup_template(args.name, args.description)
    elif args.command == "select":
        select_template(args.name, args.force)
    elif args.command == "list":
        list_templates()
    elif args.command == "delete":
        delete_template(args.name, args.force)
    elif args.command == "status":
        show_status()


if __name__ == "__main__":
    main()
