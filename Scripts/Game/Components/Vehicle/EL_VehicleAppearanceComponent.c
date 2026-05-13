[ComponentEditorProps(category: "EL/Vehicle", description: "Manages vehicle appearance and color")]
class EL_VehicleAppearanceComponentClass : ScriptComponentClass
{
}

class EL_VehicleAppearanceComponent : ScriptComponent
{
	[RplProp(onRplName: "OnVehicleColorChanged")]
	protected int m_iVehicleColor = Color.White.PackToInt();

	[RplProp(onRplName: "OnVehicleTextureChanged")]
	protected ResourceName m_VehicleTexture = "";

	//------------------------------------------------------------------------------------------------
	void SetVehicleColor(int color)
	{
		m_iVehicleColor = color;
		ApplyColor();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int GetVehicleColor()
	{
		return m_iVehicleColor;
	}

	//------------------------------------------------------------------------------------------------
	void SetVehicleTexture(ResourceName texture)
	{
		m_VehicleTexture = texture;
		ApplyTexture();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetVehicleTexture()
	{
		return m_VehicleTexture;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnVehicleColorChanged()
	{
		ApplyColor();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnVehicleTextureChanged()
	{
		ApplyTexture();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyColor()
	{
		Color color = Color.FromInt(m_iVehicleColor);
		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Apply color to main vehicle mesh
		EL_Utils.ChangeColorRecursive(owner, color);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyTexture()
	{
		// TODO: Implement texture application if needed
		// This would require material manipulation
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (GetGame().InPlayMode())
		{
			ApplyColor();
			ApplyTexture();
		}
	}
}
