#include "legocharactermanager.h"

#include "3dmanager/lego3dmanager.h"
#include "legoactors.h"
#include "legoanimactor.h"
#include "legobuildingmanager.h"
#include "legoextraactor.h"
#include "legogamestate.h"
#include "legoplantmanager.h"
#include "legovideomanager.h"
#include "misc.h"
#include "misc/legocontainer.h"
#include "misc/legostorage.h"
#include "mxmisc.h"
#include "mxvariabletable.h"
#include "realtime/realtime.h"
#include "roi/legolod.h"
#include "viewmanager/viewmanager.h"

#include <SDL3/SDL_stdinc.h>
#include <assert.h>
#include <stdio.h>
#include <vec.h>

DECOMP_SIZE_ASSERT(LegoCharacter, 0x08)
DECOMP_SIZE_ASSERT(LegoCharacterManager, 0x08)
DECOMP_SIZE_ASSERT(CustomizeAnimFileVariable, 0x24)

// GLOBAL: LEGO1 0x100fc4d0
MxU32 LegoCharacterManager::g_maxMove = 4;

// GLOBAL: LEGO1 0x100fc4d4
MxU32 LegoCharacterManager::g_maxSound = 9;

// GLOBAL: LEGO1 0x100fc4e0
MxU32 g_characterAnimationId = 10;

// GLOBAL: LEGO1 0x100fc4e4
char* LegoCharacterManager::g_customizeAnimFile = NULL;

// GLOBAL: LEGO1 0x100fc4d8
MxU32 g_soundIdOffset = 50;

// GLOBAL: LEGO1 0x100fc4dc
MxU32 g_soundIdMoodOffset = 66;

// GLOBAL: LEGO1 0x100fc4e8
MxU32 g_headTextureCounter = 0;

// GLOBAL: LEGO1 0x100fc4ec
MxU32 g_infohatVariantCounter = 2;

// GLOBAL: LEGO1 0x100fc4f0
MxU32 g_autoRoiCounter = 0;

// GLOBAL: LEGO1 0x10104f20
LegoActorInfo g_actorInfo[66];

// FUNCTION: LEGO1 0x10082a20
// FUNCTION: BETA10 0x10073c60
LegoCharacterManager::LegoCharacterManager()
{
	m_characters = new LegoCharacterMap();
	Init(); // DECOMP: inlined here in BETA10

	m_customizeAnimFile = new CustomizeAnimFileVariable("CUSTOMIZE_ANIM_FILE");
	VariableTable()->SetVariable(m_customizeAnimFile);
}

// FUNCTION: LEGO1 0x10083180
// FUNCTION: BETA10 0x10073dad
LegoCharacterManager::~LegoCharacterManager()
{
	LegoCharacter* character = NULL;
	LegoCharacterMap::iterator it;

	for (it = m_characters->begin(); it != m_characters->end(); it++) {
		character = (*it).second;

		RemoveROI(character->m_roi);

		delete[] (*it).first;
		delete (*it).second;
	}

	delete m_characters;
	delete[] g_customizeAnimFile;
}

// FUNCTION: LEGO1 0x10083270
void LegoCharacterManager::Init()
{
	for (MxS32 i = 0; i < sizeOfArray(g_actorInfo); i++) {
		g_actorInfo[i] = g_actorInfoInit[i];
	}
}

// FUNCTION: LEGO1 0x100832a0
void LegoCharacterManager::ReleaseAllActors()
{
	for (MxS32 i = 0; i < sizeOfArray(g_actorInfo); i++) {
		LegoActorInfo* info = GetActorInfo(g_actorInfo[i].m_name);

		if (info != NULL) {
			LegoExtraActor* actor = info->m_actor;

			if (actor != NULL && actor->IsA("LegoExtraActor")) {
				LegoROI* roi = g_actorInfo[i].m_roi;
				MxU32 refCount = GetRefCount(roi);

				while (refCount != 0) {
					ReleaseActor(roi);
					refCount = GetRefCount(roi);
				}
			}
		}
	}
}

// FUNCTION: LEGO1 0x10083310
MxResult LegoCharacterManager::Write(LegoStorage* p_storage)
{
	MxResult result = FAILURE;

	for (MxS32 i = 0; i < sizeOfArray(g_actorInfo); i++) {
		LegoActorInfo* info = &g_actorInfo[i];

		if (p_storage->Write(&info->m_sound, sizeof(info->m_sound)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Write(&info->m_move, sizeof(info->m_move)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Write(&info->m_mood, sizeof(info->m_mood)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Write(
				&info->m_parts[c_infohatPart].m_partNameIndex,
				sizeof(info->m_parts[c_infohatPart].m_partNameIndex)
			) != SUCCESS) {
			goto done;
		}
		if (p_storage->Write(
				&info->m_parts[c_infohatPart].m_nameIndex,
				sizeof(info->m_parts[c_infohatPart].m_nameIndex)
			) != SUCCESS) {
			goto done;
		}
		if (p_storage->Write(
				&info->m_parts[c_infogronPart].m_nameIndex,
				sizeof(info->m_parts[c_infogronPart].m_nameIndex)
			) != SUCCESS) {
			goto done;
		}
		if (p_storage->Write(
				&info->m_parts[c_armlftPart].m_nameIndex,
				sizeof(info->m_parts[c_armlftPart].m_nameIndex)
			) != SUCCESS) {
			goto done;
		}
		if (p_storage->Write(&info->m_parts[c_armrtPart].m_nameIndex, sizeof(info->m_parts[c_armrtPart].m_nameIndex)) !=
			SUCCESS) {
			goto done;
		}
		if (p_storage->Write(
				&info->m_parts[c_leglftPart].m_nameIndex,
				sizeof(info->m_parts[c_leglftPart].m_nameIndex)
			) != SUCCESS) {
			goto done;
		}
		if (p_storage->Write(&info->m_parts[c_legrtPart].m_nameIndex, sizeof(info->m_parts[c_legrtPart].m_nameIndex)) !=
			SUCCESS) {
			goto done;
		}
	}

	result = SUCCESS;

done:
	return result;
}

// FUNCTION: LEGO1 0x100833f0
MxResult LegoCharacterManager::Read(LegoStorage* p_storage)
{
	MxResult result = FAILURE;

	for (MxS32 i = 0; i < sizeOfArray(g_actorInfo); i++) {
		LegoActorInfo* info = &g_actorInfo[i];

		if (p_storage->Read(&info->m_sound, sizeof(MxS32)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_move, sizeof(MxS32)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_mood, sizeof(MxU8)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_parts[c_infohatPart].m_partNameIndex, sizeof(MxU8)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_parts[c_infohatPart].m_nameIndex, sizeof(MxU8)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_parts[c_infogronPart].m_nameIndex, sizeof(MxU8)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_parts[c_armlftPart].m_nameIndex, sizeof(MxU8)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_parts[c_armrtPart].m_nameIndex, sizeof(MxU8)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_parts[c_leglftPart].m_nameIndex, sizeof(MxU8)) != SUCCESS) {
			goto done;
		}
		if (p_storage->Read(&info->m_parts[c_legrtPart].m_nameIndex, sizeof(MxU8)) != SUCCESS) {
			goto done;
		}
	}

	result = SUCCESS;

done:
	return result;
}

// FUNCTION: LEGO1 0x100834d0
// FUNCTION: BETA10 0x100742eb
const char* LegoCharacterManager::GetActorName(MxS32 p_index)
{
	if (p_index < sizeOfArray(g_actorInfo)) {
		return g_actorInfo[p_index].m_name;
	}
	else {
		return NULL;
	}
}

// FUNCTION: LEGO1 0x100834f0
// FUNCTION: BETA10 0x1007432a
MxU32 LegoCharacterManager::GetNumActors()
{
	return sizeOfArray(g_actorInfo);
}

// FUNCTION: LEGO1 0x10083500
// FUNCTION: BETA10 0x10074345
LegoROI* LegoCharacterManager::GetActorROI(const char* p_name, MxBool p_createEntity)
{
	LegoCharacter* character = NULL;
	LegoCharacterMap::const_iterator it = m_characters->find(const_cast<char*>(p_name));

	if (!(it == m_characters->end())) {
		character = (*it).second;
		character->AddRef();
	}

	if (character == NULL) {
		LegoROI* roi = CreateActorROI(p_name);

		if (roi != NULL) {
			roi->SetVisibility(FALSE);

			character = new LegoCharacter(roi);
			char* name = new char[strlen(p_name) + 1];

			if (name != NULL) {
				strcpy(name, p_name);
				(*m_characters)[name] = character;
				VideoManager()->Get3DManager()->Add(*roi);
			}
		}
	}
	else {
		VideoManager()->Get3DManager()->Remove(*character->m_roi);
		VideoManager()->Get3DManager()->Add(*character->m_roi);
	}

	if (character != NULL) {
		if (p_createEntity && character->m_roi->GetEntity() == NULL) {
			LegoExtraActor* actor = new LegoExtraActor();

			actor->SetROI(character->m_roi, FALSE, FALSE);
			actor->SetType(LegoEntity::e_actor);
			actor->SetFlag(LegoEntity::c_managerOwned);
			GetActorInfo(p_name)->m_actor = actor;
		}

		return character->m_roi;
	}
	else {
		return NULL;
	}
}

// FUNCTION: LEGO1 0x10083b20
// FUNCTION: BETA10 0x10074608
MxBool LegoCharacterManager::Exists(const char* p_name)
{
	LegoCharacterMap::iterator it = m_characters->find(const_cast<char*>(p_name));

	if (it != m_characters->end()) {
		return TRUE;
	}

	return FALSE;
}

// FUNCTION: LEGO1 0x10083bc0
MxU32 LegoCharacterManager::GetRefCount(LegoROI* p_roi)
{
	LegoCharacterMap::iterator it;

	for (it = m_characters->begin(); it != m_characters->end(); it++) {
		LegoCharacter* character = (*it).second;
		LegoROI* roi = character->m_roi;

		if (roi == p_roi) {
			return character->m_refCount;
		}
	}

	return 0;
}

// FUNCTION: LEGO1 0x10083c30
// FUNCTION: BETA10 0x10074701
void LegoCharacterManager::ReleaseActor(const char* p_name)
{
	LegoCharacter* character = NULL;
	LegoCharacterMap::iterator it = m_characters->find(const_cast<char*>(p_name));

	if (it != m_characters->end()) {
		character = (*it).second;

		if (character->RemoveRef() == 0) {
			LegoActorInfo* info = GetActorInfo(p_name);
			LegoEntity* entity = character->m_roi->GetEntity();

			if (entity != NULL) {
				entity->SetROI(NULL, FALSE, FALSE);
			}

			RemoveROI(character->m_roi);

			delete[] (*it).first;
			delete (*it).second;

			m_characters->erase(it);

			if (info != NULL) {
				if (info->m_actor != NULL) {
					info->m_actor->ClearFlag(LegoEntity::c_managerOwned);
					delete info->m_actor;
				}
				else if (entity != NULL && entity->GetFlagsIsSet(LegoEntity::c_managerOwned)) {
					entity->ClearFlag(LegoEntity::c_managerOwned);
					delete entity;
				}

				info->m_roi = NULL;
				info->m_actor = NULL;
			}
		}
	}
}

// FUNCTION: LEGO1 0x10083db0
void LegoCharacterManager::ReleaseActor(LegoROI* p_roi)
{
	LegoCharacter* character = NULL;
	LegoCharacterMap::iterator it;

	for (it = m_characters->begin(); it != m_characters->end(); it++) {
		character = (*it).second;

		if (character->m_roi == p_roi) {
			if (character->RemoveRef() == 0) {
				LegoActorInfo* info = GetActorInfo(character->m_roi->GetName());
				LegoEntity* entity = character->m_roi->GetEntity();

				if (entity != NULL) {
					entity->SetROI(NULL, FALSE, FALSE);
				}

				RemoveROI(character->m_roi);

				delete[] (*it).first;
				delete (*it).second;

				m_characters->erase(it);

				if (info != NULL) {
					if (info->m_actor != NULL) {
						info->m_actor->ClearFlag(LegoEntity::c_managerOwned);
						delete info->m_actor;
					}
					else if (entity != NULL && entity->GetFlagsIsSet(LegoEntity::c_managerOwned)) {
						entity->ClearFlag(LegoEntity::c_managerOwned);
						delete entity;
					}

					info->m_roi = NULL;
					info->m_actor = NULL;
				}
			}

			return;
		}
	}
}

// FUNCTION: LEGO1 0x10083f10
void LegoCharacterManager::ReleaseAutoROI(LegoROI* p_roi)
{
	LegoCharacter* character = NULL;
	LegoCharacterMap::iterator it;

	for (it = m_characters->begin(); it != m_characters->end(); it++) {
		character = (*it).second;

		if (character->m_roi == p_roi) {
			if (character->RemoveRef() == 0) {
				LegoEntity* entity = character->m_roi->GetEntity();

				if (entity != NULL) {
					entity->SetROI(NULL, FALSE, FALSE);
				}

				RemoveROI(character->m_roi);

				delete[] (*it).first;
				delete (*it).second;

				m_characters->erase(it);

				if (entity != NULL && entity->GetFlagsIsSet(LegoEntity::c_managerOwned)) {
					entity->ClearFlag(LegoEntity::c_managerOwned);
					delete entity;
				}
			}

			return;
		}
	}
}

// FUNCTION: LEGO1 0x10084010
// FUNCTION: BETA10 0x10074e20
void LegoCharacterManager::RemoveROI(LegoROI* p_roi)
{
	VideoManager()->Get3DManager()->Remove(*p_roi);
}

// FUNCTION: LEGO1 0x10084030
// FUNCTION: BETA10 0x10074e4f
LegoROI* LegoCharacterManager::CreateActorROI(const char* p_key)
{
	MxBool success = FALSE;
	LegoROI* roi = NULL;
	BoundingSphere boundingSphere;
	BoundingBox boundingBox;
	MxMatrix mat;
	CompoundObject* comp;
	MxS32 i;

	Tgl::Renderer* renderer = VideoManager()->GetRenderer();
	ViewLODListManager* lodManager = GetViewLODListManager();
	LegoTextureContainer* textureContainer = TextureContainer();
	LegoActorInfo* info = GetActorInfo(p_key);

	if (info == NULL) {
		goto done;
	}

	if (!SDL_strcasecmp(p_key, "pep")) {
		LegoActorInfo* pepper = GetActorInfo("pepper");

		info->m_sound = pepper->m_sound;
		info->m_move = pepper->m_move;
		info->m_mood = pepper->m_mood;

		for (i = 0; i < sizeOfArray(info->m_parts); i++) {
			info->m_parts[i] = pepper->m_parts[i];
		}
	}

	roi = new LegoROI(renderer);
	roi->SetName(p_key);

	boundingSphere.Center()[0] = g_actorLODs[c_topLOD].m_boundingSphere[0];
	boundingSphere.Center()[1] = g_actorLODs[c_topLOD].m_boundingSphere[1];
	boundingSphere.Center()[2] = g_actorLODs[c_topLOD].m_boundingSphere[2];
	boundingSphere.Radius() = g_actorLODs[c_topLOD].m_boundingSphere[3];
	roi->SetBoundingSphere(boundingSphere);

	boundingBox.Min()[0] = g_actorLODs[c_topLOD].m_boundingBox[0];
	boundingBox.Min()[1] = g_actorLODs[c_topLOD].m_boundingBox[1];
	boundingBox.Min()[2] = g_actorLODs[c_topLOD].m_boundingBox[2];
	boundingBox.Max()[0] = g_actorLODs[c_topLOD].m_boundingBox[3];
	boundingBox.Max()[1] = g_actorLODs[c_topLOD].m_boundingBox[4];
	boundingBox.Max()[2] = g_actorLODs[c_topLOD].m_boundingBox[5];
	roi->SetBoundingBox(boundingBox);

	comp = new CompoundObject();
	roi->SetComp(comp);

	for (i = 0; i < sizeOfArray(g_actorLODs) - 1; i++) {
		char lodName[256];
		LegoActorInfo::Part& part = info->m_parts[i];

		const char* parentName;
		if (i == 0 || i == 1) {
			parentName = part.m_partName[part.m_partNameIndices[part.m_partNameIndex]];
		}
		else {
			parentName = g_actorLODs[i + 1].m_parentName;
		}

		ViewLODList* lodList = lodManager->Lookup(parentName);
		MxS32 lodSize = lodList->Size();
		sprintf(lodName, "%s%d", p_key, i);
		ViewLODList* dupLodList = lodManager->Create(lodName, lodSize);

		for (MxS32 j = 0; j < lodSize; j++) {
			LegoLOD* lod = (LegoLOD*) (*lodList)[j];
			LegoLOD* clone = lod->Clone(renderer);
			dupLodList->PushBack(clone);
		}

		lodList->Release();
		lodList = dupLodList;

		LegoROI* childROI = new LegoROI(renderer, lodList);
		lodList->Release();

		childROI->SetName(g_actorLODs[i + 1].m_name);
		childROI->SetParentROI(roi);

		BoundingSphere childBoundingSphere;
		childBoundingSphere.Center()[0] = g_actorLODs[i + 1].m_boundingSphere[0];
		childBoundingSphere.Center()[1] = g_actorLODs[i + 1].m_boundingSphere[1];
		childBoundingSphere.Center()[2] = g_actorLODs[i + 1].m_boundingSphere[2];
		childBoundingSphere.Radius() = g_actorLODs[i + 1].m_boundingSphere[3];
		childROI->SetBoundingSphere(childBoundingSphere);

		BoundingBox childBoundingBox;
		childBoundingBox.Min()[0] = g_actorLODs[i + 1].m_boundingBox[0];
		childBoundingBox.Min()[1] = g_actorLODs[i + 1].m_boundingBox[1];
		childBoundingBox.Min()[2] = g_actorLODs[i + 1].m_boundingBox[2];
		childBoundingBox.Max()[0] = g_actorLODs[i + 1].m_boundingBox[3];
		childBoundingBox.Max()[1] = g_actorLODs[i + 1].m_boundingBox[4];
		childBoundingBox.Max()[2] = g_actorLODs[i + 1].m_boundingBox[5];
		childROI->SetBoundingBox(childBoundingBox);

		CalcLocalTransform(
			Mx3DPointFloat(g_actorLODs[i + 1].m_position),
			Mx3DPointFloat(g_actorLODs[i + 1].m_direction),
			Mx3DPointFloat(g_actorLODs[i + 1].m_up),
			mat
		);
		childROI->WrappedSetLocal2WorldWithWorldDataUpdate(mat);

		if (g_actorLODs[i + 1].m_flags & LegoActorLOD::c_useTexture &&
			(i != 0 || part.m_partNameIndices[part.m_partNameIndex] != 0)) {

			LegoTextureInfo* textureInfo = textureContainer->Get(part.m_names[part.m_nameIndices[part.m_nameIndex]]);

			if (textureInfo != NULL) {
				childROI->SetTextureInfo(textureInfo);
				childROI->SetLodColor(1.0F, 1.0F, 1.0F, 0.0F);
			}
		}
		else if (g_actorLODs[i + 1].m_flags & LegoActorLOD::c_useColor || (i == 0 && part.m_partNameIndices[part.m_partNameIndex] == 0)) {
			LegoFloat red, green, blue, alpha;
			childROI->GetRGBAColor(part.m_names[part.m_nameIndices[part.m_nameIndex]], red, green, blue, alpha);
			childROI->SetLodColor(red, green, blue, alpha);
		}

		comp->push_back(childROI);
	}

	CalcLocalTransform(
		Mx3DPointFloat(g_actorLODs[c_topLOD].m_position),
		Mx3DPointFloat(g_actorLODs[c_topLOD].m_direction),
		Mx3DPointFloat(g_actorLODs[c_topLOD].m_up),
		mat
	);
	roi->WrappedSetLocal2WorldWithWorldDataUpdate(mat);

	info->m_roi = roi;
	success = TRUE;

done:
	if (!success && roi != NULL) {
		delete roi;
		roi = NULL;
	}

	return roi;
}

// FUNCTION: LEGO1 0x100849a0
// FUNCTION: BETA10 0x10075b51
MxBool LegoCharacterManager::SetHeadTexture(LegoROI* p_roi, LegoTextureInfo* p_texture)
{
	LegoResult result = SUCCESS;
	LegoROI* head = FindChildROI(p_roi, g_actorLODs[c_headLOD].m_name);

	if (head != NULL) {
		char lodName[256];

		ViewLODList* lodList = GetViewLODListManager()->Lookup(g_actorLODs[c_headLOD].m_parentName);
		assert(lodList);

		MxS32 lodSize = lodList->Size();
		sprintf(lodName, "%s%s%d", p_roi->GetName(), "head", g_headTextureCounter++);
		ViewLODList* dupLodList = GetViewLODListManager()->Create(lodName, lodSize);
		assert(dupLodList);

		Tgl::Renderer* renderer = VideoManager()->GetRenderer();

		if (p_texture == NULL) {
			LegoActorInfo* info = GetActorInfo(p_roi->GetName());
			assert(info);

			LegoActorInfo::Part& part = info->m_parts[c_headPart];
			p_texture = TextureContainer()->Get(part.m_names[part.m_nameIndices[part.m_nameIndex]]);
			assert(p_texture);
		}

		for (MxS32 i = 0; i < lodSize; i++) {
			LegoLOD* lod = (LegoLOD*) (*lodList)[i];
			LegoLOD* clone = lod->Clone(renderer);

			if (p_texture != NULL) {
				clone->UpdateTextureInfo(p_texture);
			}

			dupLodList->PushBack(clone);
		}

		lodList->Release();
		lodList = dupLodList;

		if (head->GetLodLevel() >= 0) {
			VideoManager()->Get3DManager()->GetLego3DView()->GetViewManager()->RemoveROIDetailFromScene(head);
		}

		head->SetLODList(lodList);
		lodList->Release();
	}

	return head != NULL;
}

// FUNCTION: LEGO1 0x10084c00
MxBool LegoCharacterManager::IsActor(const char* p_name)
{
	for (MxU32 i = 0; i < sizeOfArray(g_actorInfo); i++) {
		if (!SDL_strcasecmp(g_actorInfo[i].m_name, p_name)) {
			return TRUE;
		}
	}

	return FALSE;
}

// FUNCTION: LEGO1 0x10084c40
LegoExtraActor* LegoCharacterManager::GetExtraActor(const char* p_name)
{
	LegoActorInfo* info = GetActorInfo(p_name);

	if (info != NULL) {
		return info->m_actor;
	}

	return NULL;
}

// FUNCTION: LEGO1 0x10084c60
// FUNCTION: BETA10 0x10075ede
LegoActorInfo* LegoCharacterManager::GetActorInfo(const char* p_name)
{
	MxU32 i;

	for (i = 0; i < sizeOfArray(g_actorInfo); i++) {
		if (!SDL_strcasecmp(g_actorInfo[i].m_name, p_name)) {
			break;
		}
	}

	if (i < sizeOfArray(g_actorInfo)) {
		return &g_actorInfo[i];
	}
	else {
		return NULL;
	}
}

// FUNCTION: LEGO1 0x10084cb0
// FUNCTION: BETA10 0x10075f66
LegoActorInfo* LegoCharacterManager::GetActorInfo(LegoROI* p_roi)
{
	MxU32 i;

	for (i = 0; i < sizeOfArray(g_actorInfo); i++) {
		if (g_actorInfo[i].m_roi == p_roi) {
			break;
		}
	}

	if (i < sizeOfArray(g_actorInfo)) {
		return &g_actorInfo[i];
	}
	else {
		return NULL;
	}
}

// FUNCTION: LEGO1 0x10084cf0
// FUNCTION: BETA10 0x10075fe2
LegoROI* LegoCharacterManager::FindChildROI(LegoROI* p_roi, const char* p_name)
{
#ifdef COMPAT_MODE
	CompoundObject::const_iterator it;
#else
	CompoundObject::iterator it;
#endif

	const CompoundObject* comp = p_roi->GetComp();

	for (it = comp->begin(); it != comp->end(); it++) {
		LegoROI* roi = (LegoROI*) *it;

		if (!SDL_strcasecmp(p_name, roi->GetName())) {
			return roi;
		}
	}

	return NULL;
}

// FUNCTION: LEGO1 0x10084d50
// FUNCTION: BETA10 0x10076223
MxBool LegoCharacterManager::SwitchColor(LegoROI* p_roi, LegoROI* p_targetROI)
{
	MxS32 numParts = 10;
	const char* targetName = p_targetROI->GetName();

	MxS32 partIndex;
	for (partIndex = 0; partIndex < numParts; partIndex++) {
		if (!strcmp(targetName, g_actorLODs[partIndex + 1].m_name)) {
			break;
		}
	}

	assert(partIndex < numParts);

	MxBool findChild = TRUE;
	if (partIndex == c_clawlftPart) {
		partIndex = c_armlftPart;
	}
	else if (partIndex == c_clawrtPart) {
		partIndex = c_armrtPart;
	}
	else if (partIndex == c_headPart) {
		partIndex = c_infohatPart;
	}
	else if (partIndex == c_bodyPart) {
		partIndex = c_infogronPart;
	}
	else {
		findChild = FALSE;
	}

	if (!(g_actorLODs[partIndex + 1].m_flags & LegoActorLOD::c_useColor)) {
		return FALSE;
	}

	LegoActorInfo* info = GetActorInfo(p_roi->GetName());

	if (info == NULL) {
		return FALSE;
	}

	if (findChild) {
		p_targetROI = FindChildROI(p_roi, g_actorLODs[partIndex + 1].m_name);
	}

	LegoActorInfo::Part& part = info->m_parts[partIndex];

	part.m_nameIndex++;
	if (part.m_nameIndices[part.m_nameIndex] == 0xff) {
		part.m_nameIndex = 0;
	}

	LegoFloat red, green, blue, alpha;
	LegoROI::GetRGBAColor(part.m_names[part.m_nameIndices[part.m_nameIndex]], red, green, blue, alpha);
	p_targetROI->SetLodColor(red, green, blue, alpha);
	return TRUE;
}

// FUNCTION: LEGO1 0x10084ec0
MxBool LegoCharacterManager::SwitchVariant(LegoROI* p_roi)
{
	LegoActorInfo* info = GetActorInfo(p_roi->GetName());

	if (info == NULL) {
		return FALSE;
	}

	LegoActorInfo::Part& part = info->m_parts[c_infohatPart];

	part.m_partNameIndex++;
	MxU8 partNameIndex = part.m_partNameIndices[part.m_partNameIndex];

	if (partNameIndex == 0xff) {
		part.m_partNameIndex = 0;
		partNameIndex = part.m_partNameIndices[part.m_partNameIndex];
	}

	LegoROI* childROI = FindChildROI(p_roi, g_actorLODs[c_infohatLOD].m_name);

	if (childROI != NULL) {
		char lodName[256];

		ViewLODList* lodList = GetViewLODListManager()->Lookup(part.m_partName[partNameIndex]);
		MxS32 lodSize = lodList->Size();
		sprintf(lodName, "%s%d", p_roi->GetName(), g_infohatVariantCounter++);
		ViewLODList* dupLodList = GetViewLODListManager()->Create(lodName, lodSize);

		Tgl::Renderer* renderer = VideoManager()->GetRenderer();
		LegoFloat red, green, blue, alpha;
		LegoROI::GetRGBAColor(part.m_names[part.m_nameIndices[part.m_nameIndex]], red, green, blue, alpha);

		for (MxS32 i = 0; i < lodSize; i++) {
			LegoLOD* lod = (LegoLOD*) (*lodList)[i];
			LegoLOD* clone = lod->Clone(renderer);
			clone->SetColor(red, green, blue, alpha);
			dupLodList->PushBack(clone);
		}

		lodList->Release();
		lodList = dupLodList;

		if (childROI->GetLodLevel() >= 0) {
			VideoManager()->Get3DManager()->GetLego3DView()->GetViewManager()->RemoveROIDetailFromScene(childROI);
		}

		childROI->SetLODList(lodList);
		lodList->Release();
	}

	return TRUE;
}

// FUNCTION: LEGO1 0x10085090
// FUNCTION: BETA10 0x100766f6
MxBool LegoCharacterManager::SwitchSound(LegoROI* p_roi)
{
	MxBool result = FALSE;
	LegoActorInfo* info = GetActorInfo(p_roi);

	if (info != NULL) {
		info->m_sound++;

		if (info->m_sound >= g_maxSound) {
			info->m_sound = 0;
		}

		result = TRUE;
	}

	return result;
}

// FUNCTION: LEGO1 0x100850c0
// FUNCTION: BETA10 0x10076754
MxBool LegoCharacterManager::SwitchMove(LegoROI* p_roi)
{
	MxBool result = FALSE;
	LegoActorInfo* info = GetActorInfo(p_roi);

	if (info != NULL) {
		info->m_move++;

		if (info->m_move >= g_maxMove) {
			info->m_move = 0;
		}

		result = TRUE;
	}

	return result;
}

// FUNCTION: LEGO1 0x100850f0
// FUNCTION: BETA10 0x100767b2
MxBool LegoCharacterManager::SwitchMood(LegoROI* p_roi)
{
	MxBool result = FALSE;
	LegoActorInfo* info = GetActorInfo(p_roi);

	if (info != NULL) {
		info->m_mood++;

		if (info->m_mood > 3) {
			info->m_mood = 0;
		}

		result = TRUE;
	}

	return result;
}

// FUNCTION: LEGO1 0x10085120
// FUNCTION: BETA10 0x1007680c
MxU32 LegoCharacterManager::GetAnimationId(LegoROI* p_roi)
{
	LegoActorInfo* info = GetActorInfo(p_roi);

	if (info != NULL) {
		return info->m_move + g_characterAnimationId;
	}
	else {
		return 0;
	}
}

// FUNCTION: LEGO1 0x10085140
// FUNCTION: BETA10 0x10076855
MxU32 LegoCharacterManager::GetSoundId(LegoROI* p_roi, MxBool p_und)
{
	LegoActorInfo* info = GetActorInfo(p_roi);

	if (p_und) {
		return info->m_mood + g_soundIdMoodOffset;
	}

	if (info != NULL) {
		return info->m_sound + g_soundIdOffset;
	}
	else {
		return 0;
	}
}

// FUNCTION: LEGO1 0x10085180
// FUNCTION: BETA10 0x100768c5
MxU8 LegoCharacterManager::GetMood(LegoROI* p_roi)
{
	LegoActorInfo* info = GetActorInfo(p_roi);

	if (info != NULL) {
		return info->m_mood;
	}
	else {
		return 0;
	}
}

// FUNCTION: LEGO1 0x100851a0
void LegoCharacterManager::SetCustomizeAnimFile(const char* p_value)
{
	if (g_customizeAnimFile != NULL) {
		delete[] g_customizeAnimFile;
	}

	if (p_value != NULL) {
		g_customizeAnimFile = new char[strlen(p_value) + 1];

		if (g_customizeAnimFile != NULL) {
			strcpy(g_customizeAnimFile, p_value);
		}
	}
	else {
		g_customizeAnimFile = NULL;
	}
}

// FUNCTION: LEGO1 0x10085210
// FUNCTION: BETA10 0x10076995
LegoROI* LegoCharacterManager::CreateAutoROI(const char* p_name, const char* p_lodName, MxBool p_createEntity)
{
	LegoROI* roi = NULL;

	MxMatrix mat;
	Tgl::Renderer* renderer = VideoManager()->GetRenderer();
	ViewLODListManager* lodManager = GetViewLODListManager();
	LegoTextureContainer* textureContainer = TextureContainer();
	ViewLODList* lodList = lodManager->Lookup(p_lodName);

	if (lodList == NULL || lodList->Size() == 0) {
		return NULL;
	}

	roi = new LegoROI(renderer, lodList);

	const char* name;
	char buf[20];

	if (p_name != NULL) {
		name = p_name;
	}
	else {
		sprintf(buf, "autoROI_%d", g_autoRoiCounter++);
		name = buf;
	}

	roi->SetName(name);
	lodList->Release();

	if (roi != NULL && UpdateBoundingSphereAndBox(roi) != SUCCESS) {
		delete roi;
		roi = NULL;
	}

	if (roi != NULL) {
		roi->SetVisibility(FALSE);

		LegoCharacter* character = new LegoCharacter(roi);
		char* key = new char[strlen(name) + 1];

		if (key != NULL) {
			strcpy(key, name);
			(*m_characters)[key] = character;
			VideoManager()->Get3DManager()->Add(*roi);

			if (p_createEntity && roi->GetEntity() == NULL) {
				LegoEntity* entity = new LegoEntity();

				entity->SetROI(roi, FALSE, FALSE);
				entity->SetType(LegoEntity::e_autoROI);
				entity->SetFlag(LegoEntity::c_managerOwned);
			}
		}
	}

	return roi;
}

// FUNCTION: LEGO1 0x10085870
MxResult LegoCharacterManager::UpdateBoundingSphereAndBox(LegoROI* p_roi)
{
	MxResult result = FAILURE;

	BoundingSphere boundingSphere;
	BoundingBox boundingBox;

	const Tgl::MeshBuilder* meshBuilder = ((ViewLOD*) p_roi->GetLOD(0))->GetMeshBuilder();

	if (meshBuilder != NULL) {
		float min[3], max[3];

		FILLVEC3(min, 88888.0);
		FILLVEC3(max, -88888.0);
		meshBuilder->GetBoundingBox(min, max);

		float center[3];
		center[0] = (min[0] + max[0]) / 2.0f;
		center[1] = (min[1] + max[1]) / 2.0f;
		center[2] = (min[2] + max[2]) / 2.0f;
		SET3(boundingSphere.Center(), center);

		float radius[3];
		VMV3(radius, max, min);
		boundingSphere.Radius() = sqrt(NORMSQRD3(radius)) / 2.0;

		p_roi->SetBoundingSphere(boundingSphere);

		SET3(boundingBox.Min(), min);
		SET3(boundingBox.Max(), max);

		p_roi->SetBoundingBox(boundingBox);

		p_roi->WrappedUpdateWorldData();

		result = SUCCESS;
	}

	return result;
}

// FUNCTION: LEGO1 0x10085a80
LegoROI* LegoCharacterManager::FUN_10085a80(const char* p_name, const char* p_lodName, MxBool p_createEntity)
{
	return CreateAutoROI(p_name, p_lodName, p_createEntity);
}

// FUNCTION: LEGO1 0x10085aa0
CustomizeAnimFileVariable::CustomizeAnimFileVariable(const char* p_key)
{
	m_key = p_key;
	m_key.ToUpperCase();
}

// FUNCTION: LEGO1 0x10085b50
void CustomizeAnimFileVariable::SetValue(const char* p_value)
{
	// STRING: LEGO1 0x100fc4f4
	if (strcmp(m_key.GetData(), "CUSTOMIZE_ANIM_FILE") == 0) {
		CharacterManager()->SetCustomizeAnimFile(p_value);
		PlantManager()->SetCustomizeAnimFile(p_value);
		BuildingManager()->SetCustomizeAnimFile(p_value);
	}
}
