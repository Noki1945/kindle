-- powerd requires this property while the Amazon framework is stopped.
-- Only owns the service during the Kindle Lite session; the launcher stops it
-- before restoring Amazon. Uses the liblipclua shipped with the Kindle.
local lipc = require("liblipclua")
local handle, message, number
for attempt = 1, 20 do
    handle, message, number = lipc.init("com.lab126.kaf")
    if handle then break end
    os.execute("usleep 200000")
end
assert(handle, "Cannot register power bridge: " .. tostring(message) .. ":" .. tostring(number))
local started = assert(handle:register_int_property("frameworkStarted", "r"))
started.value = 1
io.stdout:write("READY\n")
io.stdout:flush()
while true do lipc.run_event_loop(math.huge) end
