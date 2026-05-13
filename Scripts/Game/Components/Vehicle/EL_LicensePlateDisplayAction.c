//------------------------------------------------------------------------------------------------
//! AddAction que muestra la matrícula del vehículo como etiqueta informativa a 5 metros.
//! Siempre aparece bloqueada (CanBePerformed = false) para que no se pueda ejecutar,
//! pero el texto de "razón" muestra la matrícula del vehículo.
//!
//! Esta acción se coloca en la entidad LicensePlateAction.et que el
//! EL_LicensePlateManagerComponent spawnea dinámicamente como hija del vehículo.
//!
//! Flujo visual en HUD:
//!   [Matrícula]       ← GetActionNameScript
//!   ABC-1234          ← SetCannotPerformReason (nunca ejecutable)
//------------------------------------------------------------------------------------------------
class EL_LicensePlateDisplayAction : ScriptedUserAction
{
	protected EL_LicensePlateManagerComponent m_PlateManager;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		// La entidad dueña es la LicensePlateAction (hija del vehículo).
		// El vehículo es el padre.
		if (!pOwnerEntity)
			return;

		IEntity vehicle = pOwnerEntity.GetParent();
		if (!vehicle)
			vehicle = pOwnerEntity; // Fallback: si ya estamos en la raíz

		m_PlateManager = EL_LicensePlateManagerComponent.Cast(vehicle.FindComponent(EL_LicensePlateManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Visible mientras el vehículo tenga matrícula asignada.
	//! La distancia de 5 m se controla ya en el contexto (Radius 5) del prefab;
	//! aquí lo dejamos permisivo para no duplicar la lógica.
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_PlateManager)
			return false;

		return !m_PlateManager.GetRegistration().IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! SIEMPRE bloqueada. El texto de razón muestra la matrícula para que sea visible en HUD.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_PlateManager)
		{
			SetCannotPerformReason("#DH-Cannot_NoLicensePlate");
			return false;
		}

		string reg = m_PlateManager.GetRegistration();
		if (reg.IsEmpty())
		{
			SetCannotPerformReason("#DH-Cannot_NoLicensePlate");
			return false;
		}

		SetCannotPerformReason(reg);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Texto del botón de acción.
	override bool GetActionNameScript(out string outName)
	{
		outName = "#DH-Action_LicensePlate";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Nunca se ejecuta (CanBePerformedScript siempre devuelve false), pero lo definimos
	//! por completitud.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Intencional: no hace nada, es solo informativa.
	}

	//------------------------------------------------------------------------------------------------
	//! Modo local: no necesita RPC al servidor.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
