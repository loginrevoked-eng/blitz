local sockets = require("sockets")
local logging = require("logging")

local M = {}
local SOCKET_TEST_PORT = 8080
local TCP_OPEN = 0

function M.supports(port)
    return port == SOCKET_TEST_PORT
end

function M.main(host, port)
    local probe_port = port
    if probe_port == nil then
        for _, probe in ipairs(host.tcp or {}) do
            if probe.result == TCP_OPEN and M.supports(probe.port) then
                probe_port = probe.port
                break
            end
        end
    end
    if probe_port == nil or not M.supports(probe_port) then
        return false
    end

    local socket, err = sockets.new()
    if not socket then
        logging.error("socket create failed:", err)
        return false
    end

    local ok
    ok, err = sockets.set_timeout(socket, host.timeout_ms or 2000)
    if not ok then
        logging.error("socket timeout setup failed:", err)
        sockets.close(socket)
        return false
    end

    ok, err = sockets.set_sock_option(socket, sockets.TCP_NODELAY, true)
    if not ok then
        logging.error("socket option setup failed:", err)
        sockets.close(socket)
        return false
    end

    ok, err = sockets.connect(socket, host.ip, probe_port)
    if not ok then
        logging.warn("socket connect failed:", err)
        sockets.close(socket)
        return false
    end

    local peer_port, peer_ip = sockets.peer_info(socket)
    logging.info("socket connected to", peer_ip, peer_port)

    local sent
    sent, err = sockets.send(socket, "socket module test\n")
    if not sent then
        logging.warn("socket send failed:", err)
        sockets.close(socket)
        return false
    end

    local response
    response, err = sockets.recv(socket, 1024)
    if not response then
        logging.warn("socket recv failed:", err)
        sockets.close(socket)
        return false
    end

    -- The test echo server closes after echoing, so this should return EOF.
    local rest
    rest, err = sockets.recv_all(socket)
    sockets.close(socket)
    if not rest then
        logging.warn("socket recv_all failed:", err)
        return false
    end

    logging.info("socket received", #response + #rest, "bytes")
    return true
end

return M
