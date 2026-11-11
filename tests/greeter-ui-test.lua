-- Exercise the real theme's input handlers without a compositor or display.
local root = assert(arg[1], "source directory required")
local values, actions, popups, listeners

local function publish(key, value)
    values[key] = value
    for _, fn in ipairs(listeners) do fn(key, value) end
end

local function setup()
    values = {
        ["greetd.users"] = "alice\tAlice\nbob\tBob",
        ["greetd.sessions"] = "First\tfirst\nSecond\tsecond",
        ["greetd.state"] = "idle",
        ["greetd.secret"] = "1",
    }
    actions, popups, listeners = {}, {}, {}
    shell = setmetatable({
        value = function(key, fallback) return values[key] or fallback end,
        action = function(name, argument)
            actions[#actions + 1] = { name, argument }
            if name == "greetd.login" or name == "greetd.cancel" or name == "greetd.respond" then
                publish("greetd.state", "busy")
            end
        end,
        popup = function(props) popups[props.name] = props end,
        on = function(event, fn)
            assert(event == "value")
            listeners[#listeners + 1] = fn
        end,
        text = function(text, props) return { text = text, props = props } end,
    }, { __index = function()
        return function(props) return type(props) == "table" and props or {} end
    end })
    dofile(root .. "/themes/tide/shell.lua")
    return assert(popups["tide-login"])
end

local function view(card)
    return card.view { hovered = "", width = 1920, height = 1080 }
end

local function find(node, key, value)
    if type(node) ~= "table" then return end
    if node[key] == value then return node end
    for _, child in pairs(node) do
        local found = find(child, key, value)
        if found then return found end
    end
end

-- Mouse and keyboard must agree about which states permit user/session changes.
for _, state in ipairs { "busy", "done" } do
    local card = setup()
    publish("greetd.state", state)
    local tree = view(card)
    find(tree, "id", "user:2").on_click()
    find(tree, "id", "session").on_click()
    card.on_key("Down", "")
    card.on_key("Tab", "")
    card.on_key("p", "must-not-submit")
    card.on_key("Return", "")
    assert(#actions == 0, "input accepted while " .. state)
    publish("greetd.state", "idle")
    card.on_key("Return", "")
    assert(actions[1][1] == "greetd.session" and actions[1][2] == "first")
    assert(actions[2][1] == "greetd.login" and actions[2][2] == "alice")
    assert(actions[3][1] == "greetd.respond" and actions[3][2] == "")
end

-- Public input is visible only while the corresponding PAM prompt is active,
-- even if a stale backend value incorrectly still says echo-on.
for _, state in ipairs { "idle", "error", "prompt" } do
    local card = setup()
    publish("greetd.state", state)
    publish("greetd.secret", "0")
    card.on_key("p", "test-password")
    local shown = find(view(card), "text", "test-password") ~= nil
    assert(shown == (state == "prompt"), "wrong input visibility in " .. state)
end

-- Leaving a conversation erases partially typed public input, including on
-- an asynchronous connection error, before any new password is entered.
for _, state in ipairs { "idle", "error", "done" } do
    local card = setup()
    publish("greetd.state", "prompt")
    publish("greetd.secret", "0")
    card.on_key("p", "old-public-answer")
    assert(find(view(card), "text", "old-public-answer"))
    publish("greetd.state", state)
    assert(not find(view(card), "text", "old-public-answer"))
    publish("greetd.state", "idle")
    card.on_key("Return", "")
    assert(actions[#actions][2] == "", "old answer survived " .. state)
end

-- Switching users at a prompt still works, but input waits for cancellation.
local card = setup()
publish("greetd.state", "prompt")
card.on_key("p", "alice-password")
find(view(card), "id", "user:2").on_click()
assert(#actions == 1 and actions[1][1] == "greetd.cancel")
card.on_key("p", "during-cancel")
card.on_key("Return", "")
assert(#actions == 1)
publish("greetd.state", "idle")
card.on_key("p", "bob-password")
card.on_key("Return", "")
assert(actions[3][1] == "greetd.login" and actions[3][2] == "bob")
assert(actions[4][1] == "greetd.respond" and actions[4][2] == "bob-password")

print("greeter-ui: PASS")
