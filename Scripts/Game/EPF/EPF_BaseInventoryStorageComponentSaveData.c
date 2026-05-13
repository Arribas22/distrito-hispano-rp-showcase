//! Modded override: skip broken inventory items instead of aborting the entire character save.
//! Vanilla EPF returns ERROR if a single slot item fails to serialize, killing the whole save
//! (bank balance, licenses, missions, etc.). This override logs the failure and continues.
modded class EPF_BaseInventoryStorageComponentSaveData
{
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		BaseInventoryStorageComponent storageComponent = BaseInventoryStorageComponent.Cast(component);

		m_iPriority = storageComponent.GetPriority();
		m_ePurposeFlags = storageComponent.GetPurpose();
		m_aSlots = {};

		for (int nSlot = 0, slots = storageComponent.GetSlotsCount(); nSlot < slots; nSlot++)
		{
			IEntity slotEntity = storageComponent.Get(nSlot);
			ResourceName prefab = EPF_Utils.GetPrefabName(slotEntity);
			EPF_BakedStorageChange slotChange = EPF_BakedStorageChange.Get(storageComponent, nSlot);
			EPF_PersistenceComponent slotPersistence = EPF_Component<EPF_PersistenceComponent>.Find(slotEntity);

			EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(owner);
			bool baked = EPF_BitFlags.CheckFlags(persistence.GetFlags(), EPF_EPersistenceFlags.BAKED);

			bool bakedParent = true;
			IEntity parent = owner.GetParent();
			if (parent)
			{
				EPF_PersistenceComponent parentPersistence = EPF_Component<EPF_PersistenceComponent>.Find(parent);
				if (parentPersistence)
					bakedParent = EPF_BitFlags.CheckFlags(parentPersistence.GetFlags(), EPF_EPersistenceFlags.BAKED);
			}

			if (!slotPersistence)
			{
				if (baked && bakedParent && (slotEntity || slotChange))
				{
					EPF_PersistentInventoryStorageSlot persistentSlot();
					persistentSlot.m_iSlotIndex = nSlot;
					m_aSlots.Insert(persistentSlot);
				}
				continue;
			}

			EPF_EReadResult readResult;
			EPF_EntitySaveData saveData = slotPersistence.Save(readResult);
			if (!saveData)
			{
				// ✅ SKIP broken item instead of aborting the entire character save
				PrintFormat("[EPF_InventorySave] WARNING: Skipping broken item in slot %1 (prefab=%2) - Save returned null. Character save will continue.", nSlot, prefab);
				continue;
			}

			if (attributes.m_bTrimDefaults &&
				readResult == EPF_EReadResult.DEFAULT &&
				EPF_BitFlags.CheckFlags(slotPersistence.GetFlags(), EPF_EPersistenceFlags.BAKED) &&
				baked &&
				bakedParent &&
				!slotChange)
			{
				bool canSkip = true;
				InventoryStorageSlot parentSlot = storageComponent.GetParentSlot();
				while (parentSlot)
				{
					if (EPF_BakedStorageChange.Has(parentSlot.GetStorage(), parentSlot.GetID()))
					{
						canSkip = false;
						break;
					}

					BaseInventoryStorageComponent storage = parentSlot.GetStorage();
					if (!storage)
						break;

					parentSlot = storage.GetParentSlot();
				}

				if (canSkip)
					continue;
			}

			EPF_PersistentInventoryStorageSlot persistentSlot();
			persistentSlot.m_iSlotIndex = nSlot;
			persistentSlot.m_pEntity = saveData;
			m_aSlots.Insert(persistentSlot);
		}

		if (m_aSlots.IsEmpty() && !EPF_StorageChangeDetection.IsDirty(storageComponent))
			return EPF_EReadResult.DEFAULT;

		return EPF_EReadResult.OK;
	}
}
