local FRC = "5846B0A6-E130-4B20-8FDD-5CCD70C860BD"

Macro { 
  area        = "Shell QView Tree Search";
  key         = "CtrlE"; 
  condition   = function() return Plugin.Exist(FRC) end;
  action      = function() Plugin.Call(FRC, "everything") end;
  description = "Everything integration via FRC Plugin";
}
