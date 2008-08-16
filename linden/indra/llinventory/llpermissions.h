/** 
 * @file llpermissions.h
 * @brief Permissions structures for objects.
 *
 * Copyright (c) 2002-2007, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 */

#ifndef LL_LLPERMISSIONS_H
#define LL_LLPERMISSIONS_H

#include <stdio.h>
#include <iostream>

#include "llpermissionsflags.h"
#include "llsd.h"
#include "lluuid.h"
#include "llxmlnode.h"
#include "reflective.h"

// prototypes
class LLMessageSystem;
extern void mask_to_string(U32 mask, char* str);
template<class T> class LLMetaClassT;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPermissions
//
// Class which encapsulates object and inventory permissions/ownership/etc.
//
// Permissions where originally a static state creator/owner and set
// of cap bits. Since then, it has grown to include group information,
// last owner, masks for different people. The implementation has been
// chosen such that a uuid is stored for each current/past owner, and
// a bitmask is stored for the base permissions, owner permissions,
// group permissions, and everyone else permissions.
//
// The base permissions represent the most permissive state that the
// permissions can possibly be in. Thus, if the base permissions do
// not allow copying, no one can ever copy the object. The permissions
// also maintain a tree-like hierarchy of permissions, thus, if we
// (for sake of discussions) denote more permissive as '>', then this
// is invariant:
//
// base mask >= owner mask >= group mask
//                         >= everyone mask
//                         >= next owner mask
// NOTE: the group mask does not effect everyone or next, everyone
// does not effect group or next, etc.
//
// It is considered a fair use right to move or delete any object you
// own.  Another fair use right is the ability to give away anything
// which you cannot copy. One way to look at that is that if you have
// a unique item, you can always give that one copy you have to
// someone else.
//
// Most of the bitmask is easy to understand, PERM_COPY means you can
// copy !PERM_TRANSFER means you cannot transfer, etc. Given that we
// now track the concept of 'next owner' inside of the permissions
// object, we can describe some new meta-meaning to the PERM_MODIFY
// flag. PERM_MODIFY is usually meant to note if you can change an
// item, but since we record next owner permissions, we can interpret
// a no-modify object as 'you cannot modify this object and you cannot
// make derivative works.' When evaluating functionality, and
// comparisons against permissions, keep this concept in mind for
// logical consistency.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLPermissions : public LLReflective
{
private:
	LLUUID			mCreator;				// null if object created by system
	LLUUID			mOwner;					// null if object "unowned" (owned by system)
	LLUUID			mLastOwner;				// object's last owner
	LLUUID			mGroup;					// The group association

	PermissionMask	mMaskBase;				// initially permissive, progressively AND restricted by each owner

	PermissionMask	mMaskOwner;				// set by owner, applies to owner only, restricts lower permissions
	PermissionMask	mMaskEveryone;			// set by owner, applies to everyone else

	PermissionMask	mMaskGroup;				// set by owner, applies to group that is associated with permissions

	PermissionMask mMaskNextOwner;			// set by owner, applied to base on transfer.

	// Usually set in the fixOwnership() method based on current uuid
	// values.
	bool mIsGroupOwned;

	// Correct for fair use - you can never take away the right to
	// move stuff you own, and you can never take away the right to
	// transfer something you cannot otherwise copy.
	void fixFairUse();

	// Fix internal consistency for group/agent ownership
	void fixOwnership();

public:
	static const LLPermissions DEFAULT;

	LLPermissions();						// defaults to created by system
	//~LLPermissions();

	// base initialization code
	void init(const LLUUID& creator, const LLUUID& owner,
			  const LLUUID& last_owner, const LLUUID& group);
	void initMasks(PermissionMask base, PermissionMask owner,
				   PermissionMask everyone, PermissionMask group,
				   PermissionMask next);

	//
	// ACCESSORS
	//

	// return the agent_id of the agent that created the item
	const LLUUID&	getCreator() 		const	{ return mCreator; }

	// return the agent_id of the owner. returns LLUUID::null if group
	// owned or public (a really big group).
	const LLUUID&	getOwner() 			const	{ return mOwner; }

	// return the group_id of the group associated with the
	// object. group_id == owner_id if the object is group owned.
	const LLUUID&	getGroup() 			const	{ return mGroup; }

	// return the agent_id of the last agent owner. Only returns
	// LLUUID::null if there has never been a previous owner.
	const LLUUID&	getLastOwner() 		const	{ return mLastOwner; }

	U32				getMaskBase() 		const	{ return mMaskBase; }
	U32				getMaskOwner() 		const	{ return mMaskOwner; }
	U32				getMaskGroup() 		const	{ return mMaskGroup; }
	U32				getMaskEveryone() 	const	{ return mMaskEveryone; }
	U32 getMaskNextOwner() const { return mMaskNextOwner; }

	// return TRUE if the object has any owner
	bool isOwned() const { return (mOwner.notNull() || mIsGroupOwned); }

	// return TRUE if group_id is owner.
	bool isGroupOwned() const { return mIsGroupOwned; }

	// This API returns TRUE if the object is owned at all, and FALSE
	// otherwise. If it is owned at all, owner id is filled with
	// either the owner id or the group id, and the is_group_owned
	// parameter is appropriately filled. The values of owner_id and
	// is_group_owned are not changed if the object is not owned.
	BOOL getOwnership(LLUUID& owner_id, BOOL& is_group_owned) const;

	// Gets the 'safe' owner.  This should never return LLUUID::null.
	// If no group owned, return the agent owner id normally.
	// If group owned, return the group id.
	// If not owned, return a random uuid which should have no power.
	LLUUID getSafeOwner() const;

	// return a cheap crc
	U32 getCRC32() const;


	//
	// MANIPULATORS
	//

	// Fix hierarchy of permissions, applies appropriate permissions
	// at each level to ensure that base permissions are respected,
	// and also ensures that if base cannot transfer, then group and
	// other cannot copy.
	void fix();

	// All of these methods just do exactly what they say. There is no
	// permissions checking to see if the operation is allowed, and do
	// not fix the permissions hierarchy. So please only use these
	// methods when you are know what you're doing and coding on
	// behalf of the system - ie, acting as god.
	void set(const LLPermissions& permissions);
	void setMaskBase(U32 mask)	   { mMaskBase = mask; }
	void setMaskOwner(U32 mask)	   { mMaskOwner = mask; }
	void setMaskEveryone(U32 mask) { mMaskEveryone = mask;}
	void setMaskGroup(U32 mask)	   { mMaskGroup = mask;}
	void setMaskNext(U32 mask) { mMaskNextOwner = mask; }

	// Allow accumulation of permissions. Results in the tightest
	// permissions possible. In the case of clashing UUIDs, it sets
	// the ID to LLUUID::null.
	void accumulate(const LLPermissions& perm);

	//
	// CHECKED MANIPULATORS 
	//

	// These functions return true on success.  They return false if
	// the given agent isn't allowed to make the change.  You can pass
	// LLUUID::null as the agent id if the change is being made by the
	// simulator itself, not on behalf of any agent - this will always
	// succeed. Passing in group id of LLUUID:null means no group, and
	// does not offer special permission to do anything.

	// saves last owner, sets current owner, and sets the group.
	// set is_atomic = true means that this permission represents
	// an atomic permission and not a collection of permissions.
	// Currently, the only way to have a collection is when an object
	// has inventory and is then itself rolled up into an inventory
	// item.
	BOOL setOwnerAndGroup(const LLUUID& agent, const LLUUID& owner, const LLUUID& group, bool is_atomic);	

	// saves last owner, sets owner to uuid null, sets group
	// owned. group_id must be the group of the object (that's who it
	// is being deeded to) and the object must be group
	// modify. Technically, the agent id and group id are not
	// necessary, but I wanted this function to look like the other
	// checked manipulators (since that is how it is used.) If the
	// agent is the system or (group == mGroup and group modify and
	// owner transfer) then this function will deed the permissions,
	// set the next owner mask, and return TRUE. Otherwise, no change
	// is effected, and the function returns FALSE.
	BOOL deedToGroup(const LLUUID& agent, const LLUUID& group);
	// Attempt to set or clear the given bitmask.  Returns TRUE if you
	// are allowed to modify the permissions.  If you attempt to turn
	// on bits not allowed by the base bits, the function will return
	// TRUE, but those bits will not be set.
	BOOL setBaseBits( const LLUUID& agent, BOOL set, PermissionMask bits);
	BOOL setOwnerBits( const LLUUID& agent, BOOL set, PermissionMask bits);
	BOOL setGroupBits( const LLUUID& agent, const LLUUID& group, BOOL set, PermissionMask bits);
	BOOL setEveryoneBits(const LLUUID& agent, const LLUUID& group, BOOL set, PermissionMask bits);
	BOOL setNextOwnerBits(const LLUUID& agent, const LLUUID& group, BOOL set, PermissionMask bits);

	//
	// METHODS
	//

	// All the allow* functions return true if the given agent or
	// group can perform the function. Prefer using this set of
	// operations to check permissions on an object.  These return
	// true if the given agent or group can perform the function.
	// They also return true if the object isn't owned, or the
	// requesting agent is a system agent.  See llpermissionsflags.h
	// for bits.
	BOOL allowOperationBy(PermissionBit op, const LLUUID& agent, const LLUUID& group = LLUUID::null) const;

	inline BOOL allowModifyBy(const LLUUID &agent_id) const;
	inline BOOL allowCopyBy(const LLUUID& agent_id) const;
	inline BOOL allowMoveBy(const LLUUID& agent_id) const;
	inline BOOL allowModifyBy(const LLUUID &agent_id, const LLUUID& group) const;
	inline BOOL allowCopyBy(const LLUUID& agent_id, const LLUUID& group) const;
	inline BOOL allowMoveBy(const LLUUID &agent_id, const LLUUID &group) const;

	// This somewhat specialized function is meant for testing if the
	// current owner is allowed to transfer to the specified agent id.
	inline BOOL allowTransferTo(const LLUUID &agent_id) const;

	//
	// DEPRECATED.
	//
	// These return true if the given agent can perform the function.
	// They also return true if the object isn't owned, or the
	// requesting agent is a system agent.  See llpermissionsflags.h
	// for bits.
	//BOOL	allowDeleteBy(const LLUUID& agent_id) 	const		{ return allowModifyBy(agent_id); }
	//BOOL	allowEditBy(const LLUUID& agent_id) 	const		{ return allowModifyBy(agent_id); }
	// saves last owner and sets current owner
	//BOOL setOwner(const LLUUID& agent, const LLUUID& owner);	
	// This method saves the last owner, sets the current owner to the
	// one provided, and sets the base mask as indicated.
	//BOOL setOwner(const LLUUID& agent, const LLUUID& owner, U32 new_base_mask);

	// Attempt to set or clear the given bitmask.  Returns TRUE if you
	// are allowed to modify the permissions.  If you attempt to turn
	// on bits not allowed by the base bits, the function will return
	// TRUE, but those bits will not be set.
	//BOOL setGroupBits( const LLUUID& agent, BOOL set, PermissionMask bits);
	//BOOL setEveryoneBits(const LLUUID& agent, BOOL set, PermissionMask bits);

	//
	// MISC METHODS and OPERATORS
	//

	// For messaging system support
	void	packMessage(LLMessageSystem* msg) const;
	void	unpackMessage(LLMessageSystem* msg, const char* block, S32 block_num = 0);

	// Load/save support
	BOOL	importFile(FILE* fp);
	BOOL	exportFile(FILE* fp) const;

	BOOL	importLegacyStream(std::istream& input_stream);
	BOOL	exportLegacyStream(std::ostream& output_stream) const;

	LLXMLNode *exportFileXML() const;
	bool importXML(LLXMLNode* node);

	bool operator==(const LLPermissions &rhs) const;
	bool operator!=(const LLPermissions &rhs) const;

	friend std::ostream& operator<<(std::ostream &s, const LLPermissions &perm);

	// Reflection.
	friend class LLMetaClassT<LLPermissions>;
	virtual const LLMetaClass& getMetaClass() const;
};

// Inlines
BOOL LLPermissions::allowModifyBy(const LLUUID& agent, const LLUUID& group) const
{
	return allowOperationBy(PERM_MODIFY, agent, group);
}

BOOL LLPermissions::allowCopyBy(const LLUUID& agent, const LLUUID& group) const
{
	return allowOperationBy(PERM_COPY, agent, group);
}


BOOL LLPermissions::allowMoveBy(const LLUUID& agent, const LLUUID& group) const
{
	return allowOperationBy(PERM_MOVE, agent, group);
}

BOOL LLPermissions::allowModifyBy(const LLUUID& agent) const
{
	return allowOperationBy(PERM_MODIFY, agent, LLUUID::null);
}

BOOL LLPermissions::allowCopyBy(const LLUUID& agent) const
{
	return allowOperationBy(PERM_COPY, agent, LLUUID::null);
}

BOOL LLPermissions::allowMoveBy(const LLUUID& agent) const
{
	return allowOperationBy(PERM_MOVE, agent, LLUUID::null);
}

BOOL LLPermissions::allowTransferTo(const LLUUID &agent_id) const
{
	if (mIsGroupOwned)
	{
		return allowOperationBy(PERM_TRANSFER, mGroup, mGroup);
	}
	else
	{
		return ((mOwner == agent_id) ? TRUE : allowOperationBy(PERM_TRANSFER, mOwner));
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLAggregatePermissions
//
// Class which encapsulates object and inventory permissions,
// ownership, etc. Currently, it only aggregates PERM_COPY,
// PERM_MODIFY, and PERM_TRANSFER.
//
// Usually you will construct an instance and hand the object several
// permissions masks to aggregate the copy, modify, and
// transferability into a nice trinary value.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLAggregatePermissions
{
public:
	enum EValue
	{
		AP_EMPTY = 0x00,
		AP_NONE = 0x01,
		AP_SOME = 0x02,
		AP_ALL = 0x03
	};

	// construct an empty aggregate permissions
	LLAggregatePermissions();

	// pass in a PERM_COPY, PERM_TRANSFER, etc, and get out a EValue
	// enumeration describing the current aggregate permissions.
	EValue getValue(PermissionBit bit) const;

	// returns the permissions packed into the 6 LSB of a U8:
	// 00TTMMCC
	// where TT = transfer, MM = modify, and CC = copy
	// LSB is to the right
	U8 getU8() const;

	// return TRUE is the aggregate permissions are empty, otherwise FALSE.
	BOOL isEmpty() const ;

	// pass in a PERM_COPY, PERM_TRANSFER, etc, and an EValue
	// enumeration to specifically set that value. Not implemented
	// because I'm not sure it's a useful api.
	//void setValue(PermissionBit bit, EValue);

	// Given a mask, aggregate the useful permissions.
	void aggregate(PermissionMask mask);

	// Aggregate aggregates
	void aggregate(const LLAggregatePermissions& ag);

	// message handling
	void packMessage(LLMessageSystem* msg, const char* field) const;
	void unpackMessage(LLMessageSystem* msg, const char* block, const char *field, S32 block_num = 0);

	static const LLAggregatePermissions empty;
	
	friend std::ostream& operator<<(std::ostream &s, const LLAggregatePermissions &perm);

protected:
	enum EPermIndex
	{
		PI_COPY = 0,
		PI_MODIFY = 1,
		PI_TRANSFER = 2,
		PI_END = 3,
		PI_COUNT = 3
	};
	void aggregateBit(EPermIndex idx, BOOL allowed);
	void aggregateIndex(EPermIndex idx, U8 bits);
	static EPermIndex perm2PermIndex(PermissionBit bit);

	// structure used to store the aggregate so far.
	U8 mBits[PI_COUNT];
};

// These functions convert between structured data and permissions as
// appropriate for serialization. The permissions are a map of things
// like 'creator_id', 'owner_id', etc, with the value copied from the
// permission object.
LLSD ll_create_sd_from_permissions(const LLPermissions& perm);
LLPermissions ll_permissions_from_sd(const LLSD& sd_perm);

#endif