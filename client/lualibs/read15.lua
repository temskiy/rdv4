--[[
    This is a library to read 15693 tags. It can be used something like this

    local reader = require('read15')
    local info, err = reader.read()
    if not info then
        print(err)
        return
    end
    print(info.UID)

--]]
-- Loads the commands-library
local cmds = require('commands')
local utils = require('utils')
local TIMEOUT = 2000 -- Shouldn't take longer than 2 seconds


--- Sends a USBpacket to the device
-- @param command - the usb packet to send
-- @param ignoreresponse - if set to true, we don't read the device answer packet
--      which is usually recipe for fail. If not sent, the host will wait 2s for a
--      response of type CMD_ACK
-- @return packet,nil if successfull
--         nil, errormessage if unsuccessfull
local function sendToDevice(command, ignoreresponse)
    local err = core.SendCommand(command:getBytes())
    if err then
        print(err)
        return nil, err
    end
    if ignoreresponse then return nil, nil end

    local response = core.WaitForResponseTimeout(cmds.CMD_ACK, TIMEOUT)
    return response, nil
end

local function errorString15693(number)
    local errors = {}
    errors[0x01] =  "The command is not supported"
    errors[0x02] =  "The command is not recognised"
    errors[0x03] =  "The option is not supported."
    errors[0x0f] =  "Unknown error."
    errors[0x10] =  "The specified block is not available (doesn’t exist)."
    errors[0x11] =  "The specified block is already -locked and thus cannot be locked again"
    errors[0x12] =  "The specified block is locked and its content cannot be changed."
    errors[0x13] =  "The specified block was not successfully programmed."
    errors[0x14] =  "The specified block was not successfully locked."

    return errors[number] or "Reserved for Future Use or Custom command error."
end


local function parse15693(data)
    local bytes = utils.ConvertAsciiToBytes(data)
    local tmp = utils.ConvertAsciiToHex(data)

    -- define ISO15_CRC_CHECK 0F47
    local crcStr = utils.Crc15(tmp, #tmp)

    if string.sub(crcStr, #crcStr - 3) ~= '470F' then
        print("CRC", crc )
        return nil, "CRC failed"
    end

    if bytes[1] % 2 == 1 then
        -- Above is a poor-mans bit check:
        -- recv[0] & ISO15_RES_ERROR //(0x01)
        local err = "Tag returned error %i: %s"
        err = string.format(err, bytes[1], errorString15693(bytes[1]))
        return nil, err
    end
    local uid = utils.ConvertBytesToHex( bytes, true )
    uid = uid:sub(5, #uid-4)
    return { uid = uid, }
end

-- This function does a connect and retrieves som info
-- @param dont_disconnect - if true, does not disable the field
-- @return if successfull: an table containing card info
-- @return if unsuccessfull : nil, error
local function read15693(slow, dont_readresponse)

--[[

    We start by trying this command:
    MANDATORY (present in ALL iso15693 tags) command (the example below is sent to a tag different from the above one):

        pm3> hf 15 info u
        UID=E007C1A257394244
        Tag Info: Texas Instrument; Tag-it HF-I Standard; 8x32bit
        pm3>

    From which we obtain less information than the above one.

    "260100" means
    0x26
    -- #define ISO15_REQ_SUBCARRIER_SINGLE  0x00    // Tag should respond using one subcarrier (ASK)
    -- #define ISO15_REQ_DATARATE_HIGH      0x02    // Tag should respond using high data rate
    -- #define ISO15_REQ_NONINVENTORY       0x00
    0x01
        inventory
    0x00

    --]]

    local command, result, info, err, data

    data = utils.Crc15("260100")

    command = Command:new{cmd = cmds.CMD_ISO_15693_COMMAND,
                                arg1 = #data / 2,
                                arg2 = 1,
                                arg3 = 1,
                                data = data}

    if slow then
        command.arg2 = 0
    end
    if dont_readresponse then
        command.arg3 = 0
    end

    local result, err = sendToDevice(command, dont_readresponse)
    if not result then
        print(err)
        return nil, "15693 identify: no answer"
    end

    local count, cmd, len, arg2, arg3 = bin.unpack('LLLL', result)
    if  len > 0 then
        data = string.sub(result, count, count+len-1)
        info, err = parse15693(data)
        if err then
            print(err)
            return nil, err
        end
        return info
    else
        return nil, "Failed to get response"
    end
end

---
-- Waits for a ISO15693 card to be placed within the vicinity of the reader.
-- @return if successfull: an table containing card info
-- @return if unsuccessfull : nil, error
local function waitFor15693()
    print("Waiting for card... press any key to quit")
    while not core.ukbhit() do
        res, err = read15693()
        if res then return res end
        -- err means that there was no response from card
    end
    return nil, "Aborted by user"
end
local library = {
    read = read15693,
    waitFor15693 = waitFor15693,
    parse15693 = parse15693,
    sendToDevice = sendToDevice,
}

return library
