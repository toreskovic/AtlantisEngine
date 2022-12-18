print("Lua thing loaded")

local bunnyTex = GetTexture("Assets/wabbit_alpha.png")

function createBunny()
    local e = NewEntity()
    local p = NewComponent("CPosition")
    local c = NewComponent("CColor")
    local v = NewComponent("CVelocity")
    local r = NewComponent("CRenderable")

    --print("1")

    --print(p.x)

    p.x = 100.0
    p.y = 100.0

    v.x = 20.0

    -- print(p.x)
    -- print(p.y)

    --print(c.col)

    --p:SetPropertyFloat(AName.new("x"), 100.0)
    --p:SetPropertyFloat(AName.new("y"), 100.0)

    --v:SetPropertyFloat(AName.new("x"), 20.0)

    --print("2")

    --r:SetPropertyResourceHandle(AName.new("textureHandle"), bunnyTex)
    r.textureHandle = bunnyTex

    --print("3")

    e:AddComponent(p)
    e:AddComponent(v)
    e:AddComponent(c)
    e:AddComponent(r)
end

local name_pos = AName.new("CPosition")
local name_vel = AName.new("CVelocity")

local name_x = AName.new("x")
local name_y = AName.new("y")

local name_components = { name_pos, name_vel }

function SomeSystem(world)
    local dt = GetDeltaTime()
    -- local entities = world:GetEntitiesWithComponents(name_components)

    -- for i = 1, #entities do
    --     local position = entities[i]:GetComponentOfType(name_pos)
    --     local velocity = entities[i]:GetComponentOfType(name_vel)
    --     position.x = position.x + velocity.x * dt
    -- end

    ForEntitiesWithComponents(name_components, function(entity)
        local position = entity:GetComponentOfType(name_pos)
        local velocity = entity:GetComponentOfType(name_vel)
        position.x = position.x + velocity.x * dt
    end)

    if dt < 1.0 / 60.0 then
        for i = 1, 100 do
            createBunny()
        end
    end
end

function Init()
    print("Init")
    createBunny()

    RegisterSystem(SomeSystem, { AName.new("System") }, { AName.new("BeginRender") })
end
