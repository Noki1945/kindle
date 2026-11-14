"""Apply Kindle Lite settings to an already mounted, verified kbrowser image."""
import json
from pathlib import Path


def write_file(path, content):
    temporary = path.with_name(path.name + ".kindle-lite.tmp")
    temporary.write_bytes(content)
    temporary.replace(path)

base = Path("/mnt/us/kindle-lite")
extension = Path("/opt/kbrowser/touchkit")
manifest_path = extension / "manifest.json"
manifest = json.loads(manifest_path.read_text())
for name in ("memory", "page"):
    source = base / ("browser-" + name + ".js")
    write_file(extension / ("kindle-lite-" + name + ".js"), source.read_bytes())

# Show navigation and the keyboard as soon as the HTML root exists. Waiting for
# document_idle can leave these controls unavailable while a site is loading.
kit_path = extension / "kit.js"
kit_source = kit_path.read_text()
if "kindleLiteStartTouchKit" not in kit_source:
    original_start = "(() => {\n"
    if original_start not in kit_source:
        raise RuntimeError("Unrecognized TouchKit bootstrap")
    early_start = """(function kindleLiteStartTouchKit() {
  if (!document.documentElement) {
    const pending = new MutationObserver(() => {
      if (document.documentElement) {
        pending.disconnect();
        kindleLiteStartTouchKit();
      }
    });
    pending.observe(document, { childList: true });
    return;
  }
"""
    kit_source = kit_source.replace(original_start, early_start, 1)
    write_file(kit_path, kit_source.encode())

page_script = "kindle-lite-page.js"
for entry in manifest["content_scripts"]:
    if "kit.js" in entry["js"]:
        entry["run_at"] = "document_start"
        if page_script not in entry["js"]:
            entry["js"].append(page_script)
write_file(manifest_path, (json.dumps(manifest, indent=2) + "\n").encode())
background = extension / "background.js"
source = background.read_text()
include = 'importScripts("kindle-lite-memory.js");'
if include not in source:
    write_file(background, (source + "\n" + include + "\n").encode())
write_file(Path("/etc/chromium/kbrowser.conf"), (base / "chromium-pw4.conf").read_bytes())
downloads = base / "books" / "Downloads"
downloads.mkdir(parents=True, exist_ok=True)
downloads.chmod(0o777)
policy_directory = Path("/etc/chromium/policies/managed")
policy_directory.mkdir(parents=True, exist_ok=True)
policies = {"DownloadDirectory": str(downloads), "PromptForDownloadLocation": False}
upstream_policy = policy_directory / "kbrowser.json"
if upstream_policy.exists():
    upstream_values = json.loads(upstream_policy.read_text())
    for name in policies:
        upstream_values.pop(name, None)
    write_file(upstream_policy, (json.dumps(upstream_values, indent=2) + "\n").encode())
write_file(policy_directory / "kindle-lite-downloads.json", (json.dumps(policies, indent=2) + "\n").encode())
write_file(base / "engines/browser/READY", b"configured\n")
print("Configured PW4 Chromium and inactive page memory management")
