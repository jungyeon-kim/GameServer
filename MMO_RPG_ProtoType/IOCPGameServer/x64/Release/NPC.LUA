MyID = -1
Damage = 20

function SetID( NewID )
	MyID = NewID
end

function BeginOverlap( PlayerID )
	if API_GetX(PlayerID) == API_GetX(MyID) then
		if API_GetY(PlayerID) == API_GetY(MyID) then
			if API_IsDead(MyID) == false then
				API_TakeDamage(PlayerID, MyID, Damage)
			end
		end
	end
end
