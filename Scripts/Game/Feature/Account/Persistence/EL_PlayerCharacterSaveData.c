[
	EPF_PersistentScriptedStateSettings(EL_PlayerCharacter),
	EDF_DbName()
]
class EL_PlayerCharacterSaveData : EPF_ScriptedStateSaveData
{
	string m_sPrefab;
	string m_sBeardPrefab;
	string m_sFirstName;
	string m_sLastName;
	int m_iAge;
	vector m_vLastSpawnPosition;
	vector m_vLastSpawnRotation;
	bool m_bHasSpawnedBefore;
	bool m_bWasDeadOnDisconnect;
	float m_fDeathMenuTimeRemaining;
	int m_iDeathTimestamp;
	vector m_vDeathPosition;
	
	// Teléfono
	string m_sPhoneNumber;
	ref array<string> m_aContactNames;
	ref array<string> m_aContactNumbers;

	//------------------------------------------------------------------------------------------------
	override EPF_EReadResult ReadFrom(notnull Managed scriptedState)
	{
		EL_PlayerCharacter character = EL_PlayerCharacter.Cast(scriptedState);
		if (!character)
			return EPF_EReadResult.ERROR;
			
		SetId(character.GetId());
		m_sPrefab = character.GetPrefab();
		m_sBeardPrefab = character.GetBeardPrefab();
		m_sFirstName = character.GetFirstName();
		m_sLastName = character.GetLastName();
		m_iAge = character.GetAge();
		m_vLastSpawnPosition = character.GetLastSpawnPosition();
		m_vLastSpawnRotation = character.GetLastSpawnRotation();
		m_bHasSpawnedBefore = character.HasSpawnedBefore();
		m_bWasDeadOnDisconnect = character.WasDeadOnDisconnect();
		m_fDeathMenuTimeRemaining = character.GetDeathMenuTimeRemaining();
		m_iDeathTimestamp = character.GetDeathTimestamp();
		m_vDeathPosition = character.GetDeathPosition();
		m_sPhoneNumber = character.GetPhoneNumber();
		m_aContactNames = character.GetContactNames();
		m_aContactNumbers = character.GetContactNumbers();
		
		return EPF_EReadResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override EPF_EApplyResult ApplyTo(notnull Managed scriptedState)
	{
		EL_PlayerCharacter character = EL_PlayerCharacter.Cast(scriptedState);
		if (!character)
			return EPF_EApplyResult.ERROR;
			
		character.SetId(GetId());
		character.SetPrefab(m_sPrefab);
		character.SetBeardPrefab(m_sBeardPrefab);
		character.SetFirstName(m_sFirstName);
		character.SetLastName(m_sLastName);
		character.SetAge(m_iAge);
		character.SetLastSpawnPosition(m_vLastSpawnPosition);
		character.SetLastSpawnRotation(m_vLastSpawnRotation);
		character.SetHasSpawnedBefore(m_bHasSpawnedBefore);
		character.SetWasDeadOnDisconnect(m_bWasDeadOnDisconnect);
		character.SetDeathMenuTimeRemaining(m_fDeathMenuTimeRemaining);
		character.SetDeathTimestamp(m_iDeathTimestamp);
		character.SetDeathPosition(m_vDeathPosition);
		character.SetPhoneNumber(m_sPhoneNumber);
		character.SetContactNames(m_aContactNames);
		character.SetContactNumbers(m_aContactNumbers);
		
		return EPF_EApplyResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override bool Equals(notnull EPF_ScriptedStateSaveData other)
	{
		EL_PlayerCharacterSaveData otherData = EL_PlayerCharacterSaveData.Cast(other);
		if (!otherData)
			return false;

		return GetId() == otherData.GetId() &&
			m_sPrefab == otherData.m_sPrefab &&
			m_sBeardPrefab == otherData.m_sBeardPrefab &&
			m_sFirstName == otherData.m_sFirstName &&
			m_sLastName == otherData.m_sLastName &&
			m_iAge == otherData.m_iAge &&
			m_vLastSpawnPosition == otherData.m_vLastSpawnPosition &&
			m_vLastSpawnRotation == otherData.m_vLastSpawnRotation &&
			m_bHasSpawnedBefore == otherData.m_bHasSpawnedBefore &&
			m_bWasDeadOnDisconnect == otherData.m_bWasDeadOnDisconnect &&
			m_fDeathMenuTimeRemaining == otherData.m_fDeathMenuTimeRemaining &&
			m_iDeathTimestamp == otherData.m_iDeathTimestamp &&
			m_vDeathPosition == otherData.m_vDeathPosition &&
			m_sPhoneNumber == otherData.m_sPhoneNumber;
	}
}
