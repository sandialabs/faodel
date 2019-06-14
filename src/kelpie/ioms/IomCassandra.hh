// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_IOMCASSANDRA_HH
#define KELPIE_IOMCASSANDRA_HH


#include "cassandra.h"

#include "kelpie/ioms/IomBase.hh"

namespace kelpie {
  namespace internal {

    class IomCassandra
      : public IomBase,
	public faodel::InfoInterface {

    public:

      using kvpair = std::pair< kelpie::Key, lunasa::DataObject >;

      IomCassandra() = delete;

      IomCassandra( const std::string& name, const std::map< std::string, std::string >& new_settings );

      virtual ~IomCassandra();

      rc_t GetInfo( faodel::bucket_t bucket,
		    const kelpie::Key &key,
		    kv_col_info_t *col_info ) override;

      void WriteObject( faodel::bucket_t bucket,
			const kelpie::Key &key,
			const lunasa::DataObject &ldo ) override;

      void WriteObjects( faodel::bucket_t bucket,
			 const std::vector< kvpair >& kvpairs );

      rc_t ReadObject( faodel::bucket_t bucket,
		       const kelpie::Key& key,
		       lunasa::DataObject* ldo ) override;

      constexpr static char type_str[] = "cassandra";

      std::string Type() const override { return IomCassandra::type_str; }

      void AppendWebInfo( faodel::ReplyStream rs,
			  std::string reference_link,
			  const std::map< std::string, std::string >& args ) override;

      void sstr( std::stringstream& ss, int depth = 0, int indent = 0 ) const override; 

      
    protected:

      std::string cluster_endpoint_;
      std::string keyspace_;
      std::string table_;
      std::string keyspace_table_;

      bool teardown_;
      
      CassCluster* cluster_;
      CassSession* session_;

    };

  } // namespace internal
} // namespace kelpie

#endif // KELPIE_IOMCASSANDRA_HH

			 
			 
