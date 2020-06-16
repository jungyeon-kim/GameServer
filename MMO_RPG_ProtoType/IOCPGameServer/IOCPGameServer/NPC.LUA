MyID = 9999

function SetID( NewID )
	MyID = NewID
end

function BeginOverlap( PlayerID )
	if API_GetX(PlayerID) == API_GetX(MyID) then
		if API_GetY(PlayerID) == API_GetY(MyID) then
			API_SendMsg(MyID, PlayerID, "Hello")
		end
	end
end
