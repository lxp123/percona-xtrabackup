/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/types/partition_index_impl.h"

#include "mysqld_error.h"                     // ER_*

#include "dd/impl/collection_impl.h"          // Collection
#include "dd/impl/properties_impl.h"          // Properties_impl
#include "dd/impl/transaction_impl.h"         // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"           // Raw_record
#include "dd/impl/tables/index_partitions.h"  // Index_partitions
#include "dd/impl/types/partition_impl.h"     // Partition_impl
#include "dd/impl/types/table_impl.h"         // Table_impl

#include <sstream>

using dd::tables::Index_partitions;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Partition_index implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Partition_index::OBJECT_TABLE()
{
  return Index_partitions::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Partition_index::TYPE()
{
  static Partition_index_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Partition_index_impl implementation.
///////////////////////////////////////////////////////////////////////////

Partition_index_impl::Partition_index_impl()
 :m_options(new Properties_impl()),
  m_se_private_data(new Properties_impl()),
  m_partition(NULL),
  m_index(NULL),
  m_tablespace_id(INVALID_OBJECT_ID)
{ }

///////////////////////////////////////////////////////////////////////////

Partition &Partition_index_impl::partition()
{
  return *m_partition;
}

///////////////////////////////////////////////////////////////////////////

Index &Partition_index_impl::index()
{
  return *m_index;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::set_options_raw(const std::string &options_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(options_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_options.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::set_se_private_data_raw(
                             const std::string &se_private_data_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(se_private_data_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_se_private_data.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

void Partition_index_impl::set_se_private_data(
                             const Properties &se_private_data)
{ m_se_private_data->assign(se_private_data); }

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::validate() const
{
  if (!m_partition)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Partition_index_impl::OBJECT_TABLE().name().c_str(),
             "No partition object associated with this element.");
    return true;
  }

  if (!m_index)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Partition_index_impl::OBJECT_TABLE().name().c_str(),
             "No index object associated with this element.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::restore_attributes(const Raw_record &r)
{
  if (check_parent_consistency(
        m_partition, r.read_ref_id(Index_partitions::FIELD_PARTITION_ID)))
    return true;

  m_index=
    m_partition->table_impl().get_index(
      r.read_ref_id(Index_partitions::FIELD_INDEX_ID));

  m_tablespace_id= r.read_ref_id(Index_partitions::FIELD_TABLESPACE_ID);

  set_options_raw(
    r.read_str(
      Index_partitions::FIELD_OPTIONS, ""));

  set_se_private_data_raw(
    r.read_str(
      Index_partitions::FIELD_SE_PRIVATE_DATA, ""));

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::store_attributes(Raw_record *r)
{
  return r->store(Index_partitions::FIELD_PARTITION_ID, m_partition->id()) ||
         r->store(Index_partitions::FIELD_INDEX_ID, m_index->id()) ||
         r->store(Index_partitions::FIELD_OPTIONS, *m_options) ||
         r->store(Index_partitions::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
         r->store_ref_id(Index_partitions::FIELD_TABLESPACE_ID, m_tablespace_id);
}

///////////////////////////////////////////////////////////////////////////

void
Partition_index_impl::serialize(WriterVariant *wv) const
{

}

void
Partition_index_impl::deserialize(const RJ_Document *d)
{

}

///////////////////////////////////////////////////////////////////////////

void Partition_index_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "PARTITION INDEX OBJECT: { "
    << "m_partition: {OID: " << m_partition->id() << "}; "
    << "m_index: {OID: " << m_index->id() << "}; "
    << "m_options " << m_options->raw_string() << "; "
    << "m_se_private_data " << m_se_private_data->raw_string() << "; "
    << "m_tablespace {OID: " << m_tablespace_id << "}";

  ss << " }";

  outb= ss.str();
}

/////////////////////////////////////////////////////////////////////////

void Partition_index_impl::drop()
{
  m_partition->index_collection()->remove(this);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Partition_index_impl::create_primary_key() const
{
  return Index_partitions::create_primary_key(
    m_partition->id(), m_index->id());
}

bool Partition_index_impl::has_new_primary_key() const
{
  /*
    Ideally, we should also check if index has newly generated ID.
    Unfortunately, we don't have Index_impl available here and it is
    hard to make it available.
    Since at the moment we can't have old partition object but new
    index objects the below check works OK.
    Also note that it is OK to be pessimistic and treat new key as an
    existing key. In theory, we simply get a bit higher probability of
    deadlock between two concurrent DDL as result. However in practice
    such deadlocks are impossible, since they also require two concurrent
    DDL updating metadata for the same existing partition which is not
    supported anyway.
  */
  return m_partition->has_new_primary_key();
}

///////////////////////////////////////////////////////////////////////////
// Partition_index_impl::Factory implementation.
///////////////////////////////////////////////////////////////////////////

Collection_item *Partition_index_impl::Factory::create_item() const
{
  Partition_index_impl *e= new (std::nothrow) Partition_index_impl();
  e->m_partition= m_partition;
  e->m_index= m_index;

  return e;
}

///////////////////////////////////////////////////////////////////////////

#ifndef DBUG_OFF
Partition_index_impl::
Partition_index_impl(const Partition_index_impl &src,
                     Partition_impl *parent, Index *index)
  : Weak_object(src),
    m_options(Properties_impl::parse_properties(src.m_options->raw_string())),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_partition(parent),
    m_index(index),
    m_tablespace_id(src.m_tablespace_id)
{}
#endif /* !DBUG_OFF */

///////////////////////////////////////////////////////////////////////////
//Partition_index_type implementation.
///////////////////////////////////////////////////////////////////////////

void Partition_index_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Index_partitions>();
}

///////////////////////////////////////////////////////////////////////////
}
