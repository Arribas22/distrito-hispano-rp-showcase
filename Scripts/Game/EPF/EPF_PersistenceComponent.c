// ============================================================================
// DEPRECADO — Toda la lógica se ha movido directamente al EPF vanilla.
// Archivo: EnfusionPersistenceFramework-0.6.15/scr/Scripts/Game/EPF_PersistenceComponent.c
//
// Las siguientes funciones están ahora en la clase base EPF_PersistenceComponent:
//   - MarkAsTemporaryNonPersistent()
//   - IsTemporaryNonPersistent()
//   - SetPreventDbDeletion(bool)
//   - IsPlayerCharacterId(string)
//   - DeletePermanent()
//   - OnDelete guards (client, temporary, empty-ID, prevent-deletion, player-character)
//   - Save() guard (temporary skip)
//
// Este archivo se mantiene vacío para evitar errores si algún .gproj lo referencia.
// Se puede eliminar de forma segura junto con la carpeta Scripts/Game/EPF/.
// ============================================================================
