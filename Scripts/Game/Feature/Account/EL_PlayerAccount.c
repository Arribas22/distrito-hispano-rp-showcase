class EL_PlayerAccount : EPF_PersistentScriptedState
{
	protected ref array<ref EL_PlayerCharacter> m_aCharacters = {};
	protected int m_iActiveCharacterIdx;

	//------------------------------------------------------------------------------------------------
	void AddCharacter(notnull EL_PlayerCharacter character, bool setAsActive = false)
	{
		// Max 2 characters per player
		if (m_aCharacters.Count() >= 2)
		{
			Print("Cannot add character: maximum 2 characters per player", LogLevel.WARNING);
			return;
		}
		
		int idx = m_aCharacters.Insert(character);
		if (setAsActive)
			m_iActiveCharacterIdx = idx;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetCharacterCount()
	{
		return m_aCharacters.Count();
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanCreateNewCharacter()
	{
		return m_aCharacters.Count() < 2;
	}

	//------------------------------------------------------------------------------------------------
	void RemoveCharacter(notnull EL_PlayerCharacter character)
	{
		m_aCharacters.RemoveItemOrdered(character);
	}

	//------------------------------------------------------------------------------------------------
	bool HasCharacters()
	{
		return !m_aCharacters.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	ref array<ref EL_PlayerCharacter> GetCharacters()
	{
		//Print(string.Format("[EL_PlayerAccount] GetCharacters called: m_aCharacters has %1 characters", m_aCharacters.Count()), LogLevel.NORMAL);
		
		foreach (EL_PlayerCharacter character : m_aCharacters)
		{
			if (character)
			{
				//Print(string.Format("[EL_PlayerAccount] Adding character: %1 %2", character.GetFirstName(), character.GetLastName()), LogLevel.NORMAL);
			}
			else
			{
				Print("[EL_PlayerAccount] WARNING: Found null character in m_aCharacters!", LogLevel.WARNING);
			}
		}
		//Print(string.Format("[EL_PlayerAccount] Returning %1 characters", m_aCharacters.Count()), LogLevel.NORMAL);
		return m_aCharacters;
	}

	//------------------------------------------------------------------------------------------------
	EL_PlayerCharacter GetActiveCharacter()
	{
		int count = -1;
		if (m_aCharacters)
			count = m_aCharacters.Count();
		
		Print(string.Format("[EL_PlayerAccount] GetActiveCharacter called. m_aCharacters=%1, Count=%2, m_iActiveCharacterIdx=%3", 
			m_aCharacters, 
			count, 
			m_iActiveCharacterIdx), LogLevel.NORMAL);
		
		if (!m_aCharacters || m_aCharacters.Count() == 0)
		{
			Print("[EL_PlayerAccount] GetActiveCharacter: No characters in account", LogLevel.WARNING);
			return null;
		}
		
		if (m_iActiveCharacterIdx < 0 || m_iActiveCharacterIdx >= m_aCharacters.Count())
		{
			Print(string.Format("[EL_PlayerAccount] GetActiveCharacter: Invalid index %1, resetting to 0. Character count: %2", m_iActiveCharacterIdx, m_aCharacters.Count()), LogLevel.WARNING);
			m_iActiveCharacterIdx = 0;
		}
		
		EL_PlayerCharacter character = m_aCharacters[m_iActiveCharacterIdx];
		string characterName = "NULL";
		if (character)
			characterName = character.GetFullName();
		
		//Print(string.Format("[EL_PlayerAccount] GetActiveCharacter returning: %1", characterName), LogLevel.NORMAL);
		return character;
	}

	//------------------------------------------------------------------------------------------------
	//! Busca un personaje por su UID dentro de la cuenta
	//! \param characterId El UID del personaje a buscar
	//! \return El personaje encontrado o null si no existe
	EL_PlayerCharacter GetCharacterByUID(string characterId)
	{
		if (!m_aCharacters || m_aCharacters.IsEmpty() || characterId.IsEmpty())
		{
			//Print(string.Format("[EL_PlayerAccount] GetCharacterByUID: No characters or empty UID. UID=%1", characterId), LogLevel.NORMAL);
			return null;
		}

		foreach (EL_PlayerCharacter character : m_aCharacters)
		{
			if (character && character.GetId() == characterId)
			{
				//Print(string.Format("[EL_PlayerAccount] GetCharacterByUID: Found character %1 (ID: %2)", character.GetFullName(), characterId), LogLevel.NORMAL);
				return character;
			}
		}

		Print(string.Format("[EL_PlayerAccount] GetCharacterByUID: Character with UID %1 not found in account", characterId), LogLevel.WARNING);
		return null;
	}

	//------------------------------------------------------------------------------------------------
	void SetActiveCharacter(notnull EL_PlayerCharacter character)
	{
		m_iActiveCharacterIdx = m_aCharacters.Find(character);
	}

	//------------------------------------------------------------------------------------------------
	int GetActiveCharacterIndex()
	{
		return m_iActiveCharacterIdx;
	}

	//------------------------------------------------------------------------------------------------
	void SetActiveCharacterIndex(int index)
	{
		// Validate index
		if (index < 0 || index >= m_aCharacters.Count())
		{
			Print(string.Format("[EL_PlayerAccount] WARNING: Trying to set invalid active character index %1 (array size: %2). Setting to 0.", index, m_aCharacters.Count()), LogLevel.WARNING);
			m_iActiveCharacterIdx = 0;
			return;
		}
		
		m_iActiveCharacterIdx = index;
	}

	//------------------------------------------------------------------------------------------------
	void SetCharacters(notnull array<ref EL_PlayerCharacter> characters)
	{
		m_aCharacters.Clear();
		foreach (EL_PlayerCharacter character : characters)
		{
			m_aCharacters.Insert(character);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Safe save that checks persistence state
	bool SafeSave()
	{
		// Check if persistence context is valid before saving
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager || persistenceManager.GetState() != EPF_EPersistenceManagerState.ACTIVE)
		{
			Print("[EL_PlayerAccount] Cannot save: Persistence manager not active (likely Workbench reload)", LogLevel.WARNING);
			return false;
		}
		
		Save();
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	static EL_PlayerAccount Create(string playerUid)
	{
		//Print(string.Format("[EL_PlayerAccount] Create called for UID: %1", playerUid), LogLevel.NORMAL);
		EL_PlayerAccount account = new EL_PlayerAccount();
		//Print(string.Format("[EL_PlayerAccount] After new - account is null? %1", account == null), LogLevel.NORMAL);
		
		if (!account)
		{
			Print("[EL_PlayerAccount] ERROR: Failed to create account instance!", LogLevel.ERROR);
			return null;
		}
		
		account.SetPersistentId(playerUid);
		
		// âš ï¸ CRITICAL: Llamar Save() para registrar el objeto en EPF
		// Sin esto, el objeto existe temporalmente pero EPF no lo rastrea
		account.Save();
		//Print(string.Format("[EL_PlayerAccount] Created and saved account successfully for UID: %1", playerUid), LogLevel.NORMAL);
		return account;
	}
};

//------------------------------------------------------------------------------------------------
//! PlayerCharacter - Datos de un personaje del jugador
//! NOTA: NO hereda de EPF - es un objeto simple que se serializa dentro de PlayerAccount
class EL_PlayerCharacter
{
	protected string m_sId;
	protected string m_sPrefab;
	protected string m_sBeardPrefab;
	protected string m_sFirstName;
	protected string m_sLastName;
	protected int m_iAge;
	protected vector m_vLastSpawnPosition;
	protected vector m_vLastSpawnRotation;
	protected bool m_bHasSpawnedBefore;
	protected bool m_bWasDeadOnDisconnect;
	protected float m_fDeathMenuTimeRemaining;
	protected int m_iDeathTimestamp;
	protected vector m_vDeathPosition;
	
	// --- Teléfono: número permanente + agenda de contactos ---
	protected string m_sPhoneNumber;
	protected ref array<string> m_aContactNames;
	protected ref array<string> m_aContactNumbers;

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetId(string id)
	{
		m_sId = id;
	}

	//------------------------------------------------------------------------------------------------
	string GetPrefab()
	{
		return m_sPrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetPrefab(string prefab)
	{
		m_sPrefab = prefab;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetBeardPrefab()
	{
		return m_sBeardPrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetBeardPrefab(string beardPrefab)
	{
		m_sBeardPrefab = beardPrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetFirstName()
	{
		return m_sFirstName;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetFirstName(string firstName)
	{
		m_sFirstName = firstName;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetLastName()
	{
		return m_sLastName;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetLastName(string lastName)
	{
		m_sLastName = lastName;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetFullName()
	{
		if (m_sFirstName.IsEmpty() && m_sLastName.IsEmpty())
			return "Unknown";
		
		return m_sFirstName + " " + m_sLastName;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetAge()
	{
		return m_iAge;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetAge(int age)
	{
		m_iAge = age;
	}
	
	//------------------------------------------------------------------------------------------------
	vector GetLastSpawnPosition()
	{
		return m_vLastSpawnPosition;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetLastSpawnPosition(vector position)
	{
		m_vLastSpawnPosition = position;
	}
	
	//------------------------------------------------------------------------------------------------
	vector GetLastSpawnRotation()
	{
		return m_vLastSpawnRotation;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetLastSpawnRotation(vector rotation)
	{
		m_vLastSpawnRotation = rotation;
	}
	
	//------------------------------------------------------------------------------------------------
	bool HasSpawnedBefore()
	{
		return m_bHasSpawnedBefore;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetHasSpawnedBefore(bool hasSpawned)
	{
		m_bHasSpawnedBefore = hasSpawned;
	}
	
	//------------------------------------------------------------------------------------------------
	bool WasDeadOnDisconnect()
	{
		return m_bWasDeadOnDisconnect;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetWasDeadOnDisconnect(bool wasDead)
	{
		m_bWasDeadOnDisconnect = wasDead;
	}
	
	//------------------------------------------------------------------------------------------------
	float GetDeathMenuTimeRemaining()
	{
		return m_fDeathMenuTimeRemaining;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetDeathMenuTimeRemaining(float timeRemaining)
	{
		m_fDeathMenuTimeRemaining = timeRemaining;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetDeathTimestamp()
	{
		return m_iDeathTimestamp;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetDeathTimestamp(int timestamp)
	{
		m_iDeathTimestamp = timestamp;
	}
	
	//------------------------------------------------------------------------------------------------
	vector GetDeathPosition()
	{
		return m_vDeathPosition;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetDeathPosition(vector position)
	{
		m_vDeathPosition = position;
	}
	
	//------------------------------------------------------------------------------------------------
	// Calcular el tiempo restante basado en el timestamp de muerte
	// @param totalDeathTime: Tiempo total de cuenta regresiva (ej: 30 segundos)
	// @return: Tiempo restante en segundos, o 0 si ya expirÃ³
	float CalculateRemainingDeathTime(float totalDeathTime)
	{
		if (m_iDeathTimestamp <= 0)
			return 0;
		
		// Obtener tiempo actual del sistema
		int currentTime = System.GetUnixTime();
		int elapsedSeconds = currentTime - m_iDeathTimestamp;
		
		float remaining = totalDeathTime - elapsedSeconds;
		if (remaining < 0)
			return 0;
		
		return remaining;
	}
	
	// --- Teléfono: getters/setters ---
	
	//------------------------------------------------------------------------------------------------
	string GetPhoneNumber()
	{
		return m_sPhoneNumber;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetPhoneNumber(string phoneNumber)
	{
		m_sPhoneNumber = phoneNumber;
	}
	
	//------------------------------------------------------------------------------------------------
	array<string> GetContactNames()
	{
		if (!m_aContactNames)
			m_aContactNames = {};
		return m_aContactNames;
	}
	
	//------------------------------------------------------------------------------------------------
	array<string> GetContactNumbers()
	{
		if (!m_aContactNumbers)
			m_aContactNumbers = {};
		return m_aContactNumbers;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetContactNames(array<string> names)
	{
		m_aContactNames = names;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetContactNumbers(array<string> numbers)
	{
		m_aContactNumbers = numbers;
	}
	
	//------------------------------------------------------------------------------------------------
	void AddContact(string name, string phoneNumber)
	{
		if (!m_aContactNames) m_aContactNames = {};
		if (!m_aContactNumbers) m_aContactNumbers = {};
		
		// Evitar duplicados por número
		for (int i = 0; i < m_aContactNumbers.Count(); i++)
		{
			if (m_aContactNumbers[i] == phoneNumber)
				return;
		}
		
		m_aContactNames.Insert(name);
		m_aContactNumbers.Insert(phoneNumber);
	}
	
	//------------------------------------------------------------------------------------------------
	void RemoveContact(string phoneNumber)
	{
		if (!m_aContactNumbers || !m_aContactNames) return;
		
		for (int i = 0; i < m_aContactNumbers.Count(); i++)
		{
			if (m_aContactNumbers[i] == phoneNumber)
			{
				m_aContactNumbers.Remove(i);
				m_aContactNames.Remove(i);
				return;
			}
		}
	}
	
	// --- Generación de número de teléfono ---
	
	//------------------------------------------------------------------------------------------------
	static string GeneratePhoneNumber()
	{
		// Formato español: 6XX o 7XX + 6 dígitos más
		int firstDigit = Math.RandomInt(6, 8); // 6 o 7
		string number = firstDigit.ToString();
		for (int i = 0; i < 8; i++)
			number += Math.RandomInt(0, 10).ToString();
		return number; // "711767931" — 9 dígitos sin espacios
	}
	
	//------------------------------------------------------------------------------------------------
	static string FormatPhoneNumber(string rawNumber)
	{
		if (rawNumber.Length() != 9)
			return rawNumber;
		return rawNumber.Substring(0, 3) + " " + rawNumber.Substring(3, 3) + " " + rawNumber.Substring(6, 3);
	}
	
	//------------------------------------------------------------------------------------------------
	static string CleanPhoneNumber(string number)
	{
		string clean = "";
		for (int i = 0; i < number.Length(); i++)
		{
			string ch = number.Get(i);
			if (ch != " ")
				clean += ch;
		}
		return clean;
	}

	//------------------------------------------------------------------------------------------------
	static EL_PlayerCharacter Create(ResourceName prefab)
	{
		EL_PlayerCharacter character = new EL_PlayerCharacter();
		
		// Generate unique ID for this character
		string characterId = EPF_PersistenceIdGenerator.Generate();
		character.SetId(characterId);
		
		// Convert ResourceName to string - this stores the full resource path with GUID
		character.m_sPrefab = prefab;
		character.m_sBeardPrefab = "";
		character.m_bHasSpawnedBefore = false;
		
		// Asignar número de teléfono único
		character.m_sPhoneNumber = GeneratePhoneNumber();
		character.m_aContactNames = {};
		character.m_aContactNumbers = {};
		
		return character;
	}
};
