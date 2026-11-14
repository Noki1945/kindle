// A page with user edits stays in memory to preserve drafts and form fields.
(() => {
  const api = globalThis.browser ?? globalThis.chrome;
  let edited = false;
  document.addEventListener("input", () => { edited = true; }, true);
  window.addEventListener("blur", () => {
    if (document.activeElement?.tagName === "IFRAME") edited = true;
  });
  api.runtime.onMessage.addListener((message, _sender, reply) => {
    if (message.cmd !== "kindle-lite-page-state") return;
    const hasText = Array.from(document.querySelectorAll("input, textarea, [contenteditable]"))
      .some(element => {
        if (element.matches("input") && !/^(text|search|email|url|tel|password|number)$/i.test(element.type)) return false;
        if (element.hasAttribute("contenteditable") && !element.isContentEditable) return false;
        return Boolean((element.value ?? element.textContent ?? "").trim());
      });
    reply({ canDiscard: !edited && !hasText });
  });
})();
