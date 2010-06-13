-- WeatherController
-- sets random weather every 5 min
print("WeatherController v.0.1");

local updateInterval = 300000;
local wm = WorldManager:getSingletonPtr();

while (1) do
  LuaScriptEngine.WaitMSec(updateInterval);

  cloudX = math.random();
  cloudY = math.random();
  cloudZ = math.random();
  
  local rand1 = math.random(0, 5);
  
  if rand1 == 5 then
	type = math.random(1, 5);
  else
	type = 0;
  end;
  
  wm:updateWeather(cloudX,cloudY,cloudZ,type);
end;




