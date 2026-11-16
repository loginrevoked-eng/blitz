local logging = require("logging")

local handlers = {
    require("scripts.ssh"),
    require("scripts.telnet"),
    require("scripts.rtsp"),
    require("scripts.rdp"),
    require("scripts.banners"),
}

local M = {}
local TCP_OPEN = 0

function M.main(host)
    if type(host) ~= "table" then
        return false
    end

    local any_confirmed = false
    for _, probe in ipairs(host.tcp or {}) do
        if probe.result == TCP_OPEN then
            for _, handler in ipairs(handlers) do
                if handler.supports(probe.port) then
                    local ok, confirmed = pcall(handler.main, host, probe.port)
                    if not ok then
                        logging.error("script handler failed:", confirmed)
                    elseif confirmed then
                        any_confirmed = true
                    end
                end
            end
        end
    end
    return any_confirmed
end

return M
