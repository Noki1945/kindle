-- SPDX-License-Identifier: GPL-3.0-or-later
-- Uses the unmodified Cool Reader engine supplied by KOReader, without its UI.
-- Working directory is the installed engine bundle.
dofile("setupkoenv.lua")
local cre = require("libs/libkoreader-cre")
local BB = require("ffi/blitbuffer")
local book, size = assert(arg[1]), tonumber(arg[2]) or 32
local output = assert(arg[3])
cre.initCache("/mnt/us/kindle-lite/runtime/cre-cache", 32 * 1024 * 1024, true, 2)
cre.initHyphDict("./data/hyph/")
for _, font in ipairs({
    "/usr/java/lib/fonts/Bookerly-Regular.ttf", "/usr/java/lib/fonts/Bookerly-Bold.ttf",
    "/usr/java/lib/fonts/Bookerly-Italic.ttf", "/usr/java/lib/fonts/Bookerly-BoldItalic.ttf",
    "./fonts/noto/NotoSans-Regular.ttf", "./fonts/noto/NotoSans-Bold.ttf",
}) do pcall(cre.registerFont, font) end
local doc = cre.newDocView(992, 930, 1)
doc:readDefaults()
doc:setVisiblePageCount(1, false)
doc:setFontFace("Bookerly")
doc:setFontSize(size)
doc:setIntProperty("crengine.page.margin.left", 12)
doc:setIntProperty("crengine.page.margin.right", 12)
doc:setIntProperty("crengine.page.margin.top", 8)
doc:setIntProperty("crengine.page.margin.bottom", 8)
doc:setIntProperty("crengine.doc.embedded.styles.enabled", 1)
doc:setIntProperty("crengine.doc.embedded.fonts.enabled", 1)
doc:setStringProperty("window.status.line", "0")
doc:setStyleSheet(book:lower():match("%.fb2$") and "./data/fb2.css" or "./data/epub.css", "")
assert(doc:loadDocument(book), "Book could not be opened by the layout engine")
local props = doc:getDocumentProps()
local title = props.title or props["doc.title"] or ""
assert(not title:match("^ERROR:"), title)
doc:renderDocument()
local buffer = BB.new(992, 930, BB.TYPE_BB8)
local function render(page, pointer)
    if pointer and pointer ~= "" then doc:gotoXPointer(pointer)
    else doc:gotoPage(math.max(1, math.min(tonumber(page) or 1, doc:getPages()))) end
    buffer:fill(BB.COLOR_WHITE)
    doc:drawCurrentPage(buffer, false, false, true, false)
    assert(buffer:writePNG(output) ~= false, "Cannot write rendered page")
    io.stdout:write("KINDLE_READY\t", doc:getCurrentPage(), "\t", doc:getPages(), "\t", doc:getXPointer() or "", "\n")
    io.stdout:flush()
end
render(1)
for line in io.lines() do
    local command, value = line:match("^(%w+)\t(.*)$")
    if command == "page" then render(tonumber(value))
    elseif command == "position" then render(nil, value)
    elseif line == "quit" then break end
end
buffer:free()
doc:close()
