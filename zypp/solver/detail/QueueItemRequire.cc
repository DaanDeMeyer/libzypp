/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* QueueItemRequire.cc
 *
 * Copyright (C) 2000-2002 Ximian, Inc.
 * Copyright (C) 2005 SUSE Linux Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <sstream>

#include "zypp/CapSet.h"
#include "zypp/base/Logger.h"
#include "zypp/base/String.h"
#include "zypp/base/Gettext.h"

#include "zypp/base/Algorithm.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/CapFilters.h"

#include "zypp/solver/detail/QueueItemRequire.h"
#include "zypp/solver/detail/QueueItemBranch.h"
#include "zypp/solver/detail/QueueItemUninstall.h"
#include "zypp/solver/detail/QueueItemInstall.h"
#include "zypp/solver/detail/QueueItem.h"
#include "zypp/solver/detail/ResolverContext.h"
#include "zypp/solver/detail/ResolverInfoDependsOn.h"
#include "zypp/solver/detail/ResolverInfoMisc.h"
#include "zypp/solver/detail/ResolverInfoNeededBy.h"

/////////////////////////////////////////////////////////////////////////
namespace zypp
{ ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  namespace solver
  { /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
    namespace detail
    { ///////////////////////////////////////////////////////////////////

using namespace std;

IMPL_PTR_TYPE(QueueItemRequire);

//---------------------------------------------------------------------------

ostream&
operator<<( ostream& os, const QueueItemRequire & item)
{
    os << "[Require: ";
    os << item._capability;
    if (item._requiring_item) {
	os << ", Required by " << item._requiring_item;
    }
    if (item._upgraded_item) {
	os << ", Upgrades " << item._upgraded_item;
    }
    if (item._lost_item) {
	os << ", Lost " << item._lost_item;
    }
    if (item._remove_only) os << ", Remove Only";
    if (item._is_child) os << ", Child";
    return os << "]";
}

//---------------------------------------------------------------------------

QueueItemRequire::QueueItemRequire (const ResPool & pool, const Capability & dep)
    : QueueItem (QUEUE_ITEM_TYPE_REQUIRE, pool)
    , _capability (dep)
    , _remove_only (false)
    , _is_child (false)
{
}


QueueItemRequire::~QueueItemRequire()
{
}

//---------------------------------------------------------------------------

void
QueueItemRequire::addPoolItem (PoolItem_Ref item)
{
    assert (!_requiring_item);
    _requiring_item = item;
}


//---------------------------------------------------------------------------
struct UniqTable
{
  /** Order PoolItems based on name and edition only. */
  struct Order
  {
    /** 'less then' based on name and edition */
    bool operator()( PoolItem_Ref lhs, PoolItem_Ref rhs ) const
    {
      int res = lhs->name().compare( rhs->name() );
      if ( res )
        return res == -1; // lhs < rhs ?
      // here: lhs == rhs, so compare edition:
      return lhs->edition() < rhs->edition();
    }
  };
  /** Set of PoolItems unified by Order. */
  typedef std::set<PoolItem,Order> UTable;


  /** Test whether a matching PoolItem_Ref is in the table. */
  bool has( PoolItem_Ref item_r ) const
  { return _table.find( item_r ) != _table.end(); }

  /** Remember \a item_r (unless another matching PoolItem
   *  is already in the table)
  */
  void remember( PoolItem_Ref item_r )
  { _table.insert(  item_r ); }


  /** The set. */
  UTable _table;
};
//---------------------------------------------------------------------------

struct RequireProcess : public resfilter::OnCapMatchCallbackFunctor
{
    PoolItem_Ref requirer;
    const Capability & capability;
    ResolverContext_Ptr context;
    ResPool pool;
    PoolItemList providers;		// the provider which matched
    UniqTable uniq;

    RequireProcess (PoolItem_Ref r, const Capability & c, ResolverContext_Ptr ctx, const ResPool & p)
	: requirer (r)
	, capability (c)
	, context (ctx)
	, pool (p)
    { }

    bool operator()( PoolItem_Ref provider, const Capability & match )
    {
	//const Capability match;
	ResStatus status;

	status = provider.status();
// ERR << "RequireProcessInfo (" << *provider << " provides " << match << ", is " << status << ")" << endl;
// ERR << "RequireProcessInfo(required: " << *capability << ")" << endl;
// ERR << "require_process_cb(itemIsPossible -> " <<  context->itemIsPossible (*provider) << ")" << endl;

	/* capability is set for item set childern only. If it is set
	   allow only exactly required version */

	if (capability != Capability()
	    && capability != match) {		// exact match required
	    return true;
	}

	if ((!status.isToBeUninstalled())
	    && ! context->isParallelInstall (provider)
	    && ! uniq.has(provider)
	    && context->itemIsPossible (provider)
#warning Locks not implemented
//	    && ! pool->itemIsLocked (provider)
	) {

	    providers.push_front (provider);
	    uniq.remember(provider);
	}

	return true;
    }
};


struct NoInstallableProviders : public resfilter::OnCapMatchCallbackFunctor
{
    PoolItem_Ref requirer;
    ResolverContext_Ptr context;

    bool operator()( PoolItem_Ref provider, const Capability & match )
    {
	string msg_str;
	//const Capability match;

	ResStatus status = provider.status();

	ResolverInfoMisc_Ptr misc_info;

	if (status.isToBeUninstalled()) {
	    misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_UNINSTALL_PROVIDER, requirer, RESOLVER_INFO_PRIORITY_VERBOSE, match);
	    misc_info->setOtherPoolItem (provider);
	} else if (context->isParallelInstall (provider)) {
	    misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_PARALLEL_PROVIDER, requirer, RESOLVER_INFO_PRIORITY_VERBOSE, match);
	    misc_info->setOtherPoolItem (provider);
	} else if (! context->itemIsPossible (provider)) {
	    misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_NOT_INSTALLABLE_PROVIDER, requirer, RESOLVER_INFO_PRIORITY_VERBOSE, match);
	    misc_info->setOtherPoolItem (provider);
#warning Locks not implemented
#if 0
	} else if (pool->itemIsLocked (provider)) {
	    misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_LOCKED_PROVIDER, requirer, RESOLVER_INFO_PRIORITY_VERBOSE, match);
	    misc_info->setOtherPoolItem (provider);
#endif
 	}

	if (misc_info != NULL) {
	    context->addInfo (misc_info);
	}

	return true;
    }
};


struct LookForUpgrades : public resfilter::OnCapMatchCallbackFunctor
{
    PoolItem_Ref installed;
    PoolItemList upgrades;

    LookForUpgrades (PoolItem_Ref i)
	: installed (i)
    { }

    bool operator()( PoolItem_Ref provider )
    {
	if (installed->edition().compare (provider->edition()) < 0) {
	    upgrades.push_front (provider);
	    return true;
	}
	return false;
    }
};


// check 'codependent' items
//   by looking at the name prefix: 'foo' and 'foo-bar' are codependent

static bool
codependent_items (const PoolItem_Ref item1, const PoolItem_Ref item2)
{
    string name1 = item1->name();
    string name2 = item2->name();
    string::size_type len1 = name1.size();
    string::size_type len2 = name2.size();

    if (len2 < len1) {
	string swap = name1;
	string::size_type swap_len = len1;
	name1 = name2;
	name2 = swap;
	len1 = len2;
	len2 = swap_len;
    }

    // foo and foo-bar are automatically co-dependent
    if (len1 < len2
	&& name1.compare (0, len1, name2) == 0
	&& name2[len1] == '-') {
	return true;
    }

    return false;
}


bool
QueueItemRequire::process (ResolverContext_Ptr context, QueueItemList & new_items)
{
    DBG << "QueueItemRequire::process(" << *this << ")" << endl;

    if (context->requirementIsMet (_capability, _is_child)) {
	DBG <<  "requirement is already met in current context" << endl;
	return true;
    }

    RequireProcess info (_requiring_item, _is_child ? _capability : Capability(), context,  pool());

    int num_providers = 0;

    if (! _remove_only) {

	Dep dep( Dep::PROVIDES );

	// world->foreachProvidingResItem (_capability, require_process_cb, &info);
	invokeOnEach( pool().byCapabilityIndexBegin( _capability.index(), dep ),
		      pool().byCapabilityIndexEnd( _capability.index(), dep ),
		      resfilter::callOnCapMatchIn( dep, _capability, functor::functorRef<bool,PoolItem,Capability>(info) ) );

	num_providers = info.providers.size();

	DBG << "requirement is met by " << num_providers << " resolvable";
    }

    std::string msg;

    //
    // No providers found
    //

    if (num_providers == 0) {

	DBG << "Unfulfilled requirement, try different solution" << endl;

	QueueItemUninstall_Ptr uninstall_item = NULL;
	QueueItemBranch_Ptr branch_item = NULL;
	bool explore_uninstall_branch = true;

	if (!_upgraded_item) {

	    ResolverInfo_Ptr err_info;

	    if (_remove_only) {
		err_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_NO_OTHER_PROVIDER, _requiring_item, RESOLVER_INFO_PRIORITY_VERBOSE, _capability);
	    } else {
		err_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_NO_PROVIDER, _requiring_item, RESOLVER_INFO_PRIORITY_VERBOSE, _capability);
	    }

	    context->addInfo (err_info);

	    NoInstallableProviders info;
	    info.requirer = _requiring_item;
	    info.context = context;

	    // Maybe we can add some extra info on why none of the providers are suitable.

	    // pool()->foreachProvidingResItem (_capability, no_installable_providers_info_cb, (void *)&info);

	    Dep dep( Dep::PROVIDES );

	    invokeOnEach( pool().byCapabilityIndexBegin( _capability.index(), dep ), // begin()
			  pool().byCapabilityIndexEnd( _capability.index(), dep ),   // end()
			  resfilter::callOnCapMatchIn( dep, _capability, functor::functorRef<bool,PoolItem,Capability>(info)) );

	}

	//
	// If this is an upgrade, we might be able to avoid removing stuff by upgrading it instead.
	//

	if (_upgraded_item
	    && _requiring_item) {

	    LookForUpgrades info (_requiring_item);

//	    pool()->foreachUpgrade (_requiring_item, new Channel(CHANNEL_TYPE_ANY), look_for_upgrades_cb, (void *)&upgrade_list);

	    invokeOnEach( pool().byNameBegin( _requiring_item->name() ), pool().byNameEnd( _requiring_item->name() ),
			  functor::chain( resfilter::ByUninstalled(),
					  resfilter::ByKind( _requiring_item->kind() ) ),
#if 0
	// CompareByGT is broken		  resfilter::byEdition<CompareByGT<Edition> >( _requiring_item->edition() )),
#endif
					  functor::functorRef<bool,PoolItem>(info) );

	    if (!info.upgrades.empty()) {
		string label;

		branch_item = new QueueItemBranch (pool());

		ostringstream req_str;	req_str << _requiring_item;
		ostringstream up_str; up_str << _upgraded_item;
		ostringstream cap_str; cap_str << _capability;

		 // Translator: 1.%s = dependency; 2.%s and 3.%s = name of package,patch,...
		label = str::form (_("for requiring %s for %s when upgrading %s"),
				   cap_str.str().c_str(), req_str.str().c_str(), up_str.str().c_str());
		branch_item->setLabel (label);
// ERR << "Branching: " << label << endl;
		for (PoolItemList::const_iterator iter = info.upgrades.begin(); iter != info.upgrades.end(); iter++) {
		    PoolItem_Ref upgrade_item = *iter;
		    QueueItemInstall_Ptr install_item;

		    if (context->itemIsPossible (upgrade_item)) {

			install_item = new QueueItemInstall (pool(), upgrade_item);
		    	install_item->setUpgrades (_requiring_item);
			branch_item->addItem (install_item);

			ResolverInfoNeededBy_Ptr upgrade_info = new ResolverInfoNeededBy (upgrade_item);
			upgrade_info->addRelatedPoolItem (_upgraded_item);
			install_item->addInfo (upgrade_info);

			// If an upgrade item has its requirements met, don't do the uninstall branch.
			//   FIXME: should we also look at conflicts here?

			if (explore_uninstall_branch) {
			    CapSet requires = upgrade_item->dep (Dep::REQUIRES);
			    CapSet::const_iterator iter = requires.begin();
			    for (; iter != requires.end(); iter++) {
				const Capability req = *iter;
				if (! context->requirementIsMet (req)) {
					break;
				}
			    }
			    if (iter == requires.end()) {
				explore_uninstall_branch = false;
			    }
			}

		    } /* if (context->itemIsPossible ( ... */
		} /* for (iter = upgrade_list; ... */
	    } /* if (info.upgrades) ... */

	    if (!info.upgrades.empty()
		&& branch_item->isEmpty ()) {

		for (PoolItemList::const_iterator iter = info.upgrades.begin(); iter != info.upgrades.end(); iter++) {
		    ResolverInfoMisc_Ptr misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_NO_UPGRADE, _requiring_item, RESOLVER_INFO_PRIORITY_VERBOSE);
		    if (iter == info.upgrades.begin()) {
			misc_info->setOtherPoolItem (*iter);
		    }
		    misc_info->addRelatedPoolItem (*iter);
		    context->addInfo (misc_info);

		    explore_uninstall_branch = true;
		}

		//
		//  The exception: we always want to consider uninstalling
		//  when the requirement has resulted from a item losing
		//  one of it's provides.

	    } else if (!info.upgrades.empty()
		       && explore_uninstall_branch
		       && codependent_items (_requiring_item, _upgraded_item)
		       && !_lost_item) {
		explore_uninstall_branch = false;
	    }

	} /* if (_upgrade_item && _requiring_item) ... */

	// We always consider uninstalling when in verification mode.

	if (context->verifying()) {
	    explore_uninstall_branch = true;
	}

	if (explore_uninstall_branch && _requiring_item) {
	    ResolverInfo_Ptr log_info;
	    uninstall_item = new QueueItemUninstall (pool(), _requiring_item, QueueItemUninstall::UNSATISFIED);
	    uninstall_item->setCapability (_capability);

	    if (_lost_item) {
		log_info = new ResolverInfoDependsOn (_requiring_item, _lost_item);
		uninstall_item->addInfo (log_info);
	    }

	    if (_remove_only)
		uninstall_item->setRemoveOnly ();
	}

	if (uninstall_item && branch_item) {
	    branch_item->addItem (uninstall_item);
	    new_items.push_front (branch_item);
	} else if (uninstall_item) {
	    new_items.push_front (uninstall_item);
	} else if (branch_item) {
	    new_items.push_front (branch_item);
	} else {
	    // We can't do anything to resolve the missing requirement, so we fail.

	    ResolverInfo_Ptr misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_CANT_SATISFY, _requiring_item, RESOLVER_INFO_PRIORITY_VERBOSE, _capability);
	    context->addError (misc_info);
	}

    }

    //
    // exactly 1 provider found
    //

    else if (num_providers == 1) {

	DBG << "Found exactly one resolvable, installing it." << endl;

	QueueItemInstall_Ptr install_item = new QueueItemInstall (pool(), info.providers.front());
	install_item->addDependency (_capability);

	// The requiring item could be NULL if the requirement was added as an extra dependency.
	if (_requiring_item) {
	    install_item->addNeededBy (_requiring_item);
	}
	new_items.push_front (install_item);

    }

    //
    // multiple providers found
    //

    else if (num_providers > 1) {

	      DBG << "Found more than one resolvable, branching." << endl;

// ERR << "Found more than one item, branching." << endl;
	QueueItemBranch_Ptr branch_item = new QueueItemBranch (pool());

	for (PoolItemList::const_iterator iter = info.providers.begin(); iter != info.providers.end(); iter++) {
	    QueueItemInstall_Ptr install_item = new QueueItemInstall (pool(), *iter);
	    install_item->addDependency (_capability);
	    branch_item->addItem (install_item);

	    // The requiring item could be NULL if the requirement was added as an extra dependency.
	    if (_requiring_item) {
		install_item->addNeededBy (_requiring_item);
	    }
	}

	new_items.push_front (branch_item);

    } else {
	abort ();
    }

    return true;
}

//---------------------------------------------------------------------------

QueueItem_Ptr
QueueItemRequire::copy (void) const
{
    QueueItemRequire_Ptr new_require = new QueueItemRequire (pool(), _capability);

    new_require->QueueItem::copy(this);

    new_require->_requiring_item = _requiring_item;
    new_require->_upgraded_item  = _upgraded_item;
    new_require->_remove_only    = _remove_only;

    return new_require;
}


int
QueueItemRequire::cmp (QueueItem_constPtr item) const
{
    int cmp = this->compare (item);		// assures equal type
    if (cmp != 0)
	return cmp;

    QueueItemRequire_constPtr require = dynamic_pointer_cast<const QueueItemRequire>(item);

    if (_capability != require->capability())
    {
	cmp = -1;
    }
    return cmp;
}

///////////////////////////////////////////////////////////////////
    };// namespace detail
    /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
  };// namespace solver
  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
};// namespace zypp
/////////////////////////////////////////////////////////////////////////

