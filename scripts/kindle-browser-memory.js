// Retain two recent pages and release older, unedited pages on the 512 MB PW4.
const kindleBrowserApi = globalThis.browser ?? globalThis.chrome;
let kindleDiscardTimer;

async function discardInactivePages() {
  const tabs = await kindleBrowserApi.tabs.query({});
  const loaded = tabs.filter(tab => !tab.discarded);
  let loadedCount = loaded.length;
  loaded.sort((left, right) => (left.lastAccessed ?? 0) - (right.lastAccessed ?? 0));
  for (const tab of loaded) {
    if (loadedCount <= 2) break;
    if (tab.active || tab.pinned || tab.audible || tab.status !== "complete" || tab.autoDiscardable === false) continue;
    try {
      const state = await kindleBrowserApi.tabs.sendMessage(tab.id, { cmd: "kindle-lite-page-state" });
      if (state?.canDiscard) {
        const current = await kindleBrowserApi.tabs.get(tab.id);
        if (!current.active && !current.discarded) {
          await kindleBrowserApi.tabs.discard(tab.id);
          loadedCount -= 1;
        }
      }
    } catch (_) {
      // Unknown pages and pages that disappeared retain their existing state.
    }
  }
}

function scheduleKindleDiscard() {
  clearTimeout(kindleDiscardTimer);
  kindleDiscardTimer = setTimeout(() => discardInactivePages().catch(() => {}), 5000);
}

kindleBrowserApi.tabs.onActivated.addListener(scheduleKindleDiscard);
kindleBrowserApi.tabs.onUpdated.addListener((_id, change) => {
  if (change.status === "complete") scheduleKindleDiscard();
});
scheduleKindleDiscard();
