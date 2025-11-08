// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <stdexcept>

#include "kelpie/ioms/IomCassandra.hh"



namespace kelpie {

  namespace internal {

    constexpr char IomCassandra::type_str[];
    
    // Convenience function to handle CassFuture* in std::cerr output
    std::ostream&
    operator<<( std::ostream& ostr, CassFuture* future )
    {
      const char* msg;
      size_t msg_len;
      cass_future_error_message( future, &msg, &msg_len );
      ostr << std::string( msg, msg_len );
      return ostr;
    }


    IomCassandra::IomCassandra( const std::string& name,
				const std::map< std::string, std::string >& new_settings )
      : IomBase( name, new_settings, {"endpoint","keyspace","table","teardown","cass-replication-class","cass-replication-factor"} )
    {
      const char* msg;
      size_t msg_len;
      std::string cass_replication_class, cass_replication_factor;

      auto ai = settings.find( "endpoint" );
      if( ai == settings.end() ) {
	throw std::runtime_error( "IOM " + name + " was not given a setting for 'endpoint'" );
      }
      cluster_endpoint_ = ai->second;

      ai = settings.find( "keyspace" );
      keyspace_ = ( ai == settings.end() ) ? "faodel" : ai->second;
      ai = settings.find( "table" );
      table_ = ( ai == settings.end() ) ? "ldo" : ai->second;
      keyspace_table_ = keyspace_ + "." + table_;

      teardown_ = false;
      ai = settings.find( "teardown" );      
      if( ai != settings.end() ) {
	teardown_ = ( ai->second == "true"
		     or ai->second == "yes"
		     or ai->second == "TRUE"
		     or ai->second == "YES" );
      }

      ai = settings.find( "cass-replication-class" );
      cass_replication_class = ( ai == settings.end() ) ? "SimpleStrategy" : ai->second;
      ai = settings.find( "cass-replication-factor" );
      cass_replication_factor = ( ai == settings.end() ) ? "1" : ai->second;
      
      cluster_ = cass_cluster_new();
      session_ = cass_session_new();

      cass_cluster_set_contact_points( cluster_, cluster_endpoint_.c_str() );

      CassFuture* future = cass_session_connect( session_, cluster_ );
      cass_future_wait( future );
      if( cass_future_error_code( future ) != CASS_OK ) {
	cass_future_error_message( future, &msg, &msg_len );
	throw std::runtime_error( std::string("Unable to connect to Cassandra cluster instance: ") + std::string( msg, msg_len ) );
      }
      cass_future_free( future );
      
      /* 
       * Set up the database keyspace and table if they do not already exist
       */
      const std::string keyspace_create_cql_ =
	"create keyspace if not exists "
	+ keyspace_
	+ " with replication = {'class':"
	+ "'" + cass_replication_class + "'"
	+ ",'replication_factor':"
	+ cass_replication_factor +"};";
      CassStatement* stmt = cass_statement_new( keyspace_create_cql_.c_str(), 0 );
      future = cass_session_execute( session_, stmt );
      cass_future_wait( future );
      if( cass_future_error_code( future ) not_eq CASS_OK ) {
	cass_future_error_message( future, &msg, &msg_len );
	throw std::runtime_error( std::string( "IomCassandra: unable to create keyspace: " ) + std::string( msg, msg_len ) );
      }
      cass_statement_free( stmt );
      cass_future_free( future );

      const std::string table_create_cql_ =
	"create table if not exists "
	+ keyspace_table_
	+ " ( bucket text, key text, type tinyint, meta_size bigint, data_size bigint, payload blob, primary key ( bucket, key ) );";
      stmt = cass_statement_new( table_create_cql_.c_str(), 0 );
      future = cass_session_execute( session_, stmt );
      cass_future_wait( future );
      if( cass_future_error_code( future ) not_eq CASS_OK ) {
	cass_future_error_message( future, &msg, &msg_len );
	throw std::runtime_error( std::string( "IomCassandra: unable to create table: " ) + std::string( msg, msg_len ) );
      }
      cass_statement_free( stmt );
      cass_future_free( future );
    }

    IomCassandra::~IomCassandra() {

      if( teardown_ ) {
	/* sufficient to just drop the keyspace, that'll kill any contained tables */
	const std::string drop_keyspace_cql = "drop keyspace " + keyspace_ + ";";
	CassStatement* stmt = cass_statement_new( drop_keyspace_cql.c_str(), 0 );
	CassFuture* future = cass_session_execute( session_, stmt );
	cass_future_wait( future );
	if( cass_future_error_code( future ) not_eq CASS_OK ) {
	  std::cerr << "IomCassandra: unable to drop keyspace: " << future << std::endl;
	}
	cass_statement_free( stmt );
	cass_future_free( future );
      }
      
      cass_cluster_free( cluster_ );
      cass_session_free( session_ );
    }


    /*
      The Cassandra LDO table schema (table faodel.ldo):
      const char* bucket
      const char* key
      cass_int8_t type
      cass_int64_t meta_size
      cass_int64_t data_size
      const cass_byte_t* payload
    */
   
    rc_t
    IomCassandra::WriteObject( faodel::bucket_t bucket,
			       const kelpie::Key& key,
			       const lunasa::DataObject &ldo )
    {
      try {
        internal_WriteObject( bucket, { kvpair( key, ldo ) } );
        return 0;
      } catch (std::runtime_error &e) {
        return 1;
      }
    }


    void
    IomCassandra::internal_WriteObject( faodel::bucket_t bucket,
				const std::vector< kvpair >& kvpairs )
    {
      size_t wr_amt = 0;
      CassError rc = CASS_OK;
      CassStatement* stmt = nullptr;
      CassFuture* fut = nullptr;
      CassBatch* batch;
      std::string insert_cql = "INSERT INTO " + keyspace_table_ + " (bucket, key, type, meta_size, data_size, payload) VALUES (?, ?, ?, ?, ?, ?);";

      fut = cass_session_prepare( session_, insert_cql.c_str() );
      cass_future_wait( fut );
      rc = cass_future_error_code( fut );
      if( rc not_eq CASS_OK ) {
        std::stringstream ss;
        ss << "IomCassandra::Cassandra batch write preparation failed: " << fut;
        throw std::runtime_error(ss.str());
      }

      const CassPrepared* prep = cass_future_get_prepared( fut );
      cass_future_free( fut );

      batch = cass_batch_new( CASS_BATCH_TYPE_LOGGED );
      
      for( auto &&kv : kvpairs ) {
	const kelpie::Key& key = kv.first;
	const lunasa::DataObject& ldo = kv.second;

	stmt = cass_prepared_bind( prep );
	cass_statement_bind_string( stmt, 0, bucket.GetHex().c_str() );
	cass_statement_bind_string( stmt, 1, key.str().c_str() );
	cass_statement_bind_int8( stmt, 2, ldo.GetTypeID() );
	cass_statement_bind_int64( stmt, 3, ldo.GetMetaSize() );
	cass_statement_bind_int64( stmt, 4, ldo.GetDataSize() );
	cass_statement_bind_bytes( stmt, 5, reinterpret_cast<const cass_byte_t*>( ldo.GetMetaPtr() ), ldo.GetUserSize() );
	wr_amt += ldo.GetUserSize();

	cass_batch_add_statement( batch, stmt );
	cass_statement_free( stmt );
      }

      fut = cass_session_execute_batch( session_, batch );
      cass_future_wait( fut );
      rc = cass_future_error_code( fut );
      if( rc not_eq CASS_OK ) {
        std::stringstream ss;
        ss << "IomCassandra::Cassandra batch write failed: " << fut;
        throw std::runtime_error(ss.str());
      } else {
	stat_wr_requests += kvpairs.size();
	stat_wr_bytes += wr_amt;
      }

      cass_future_free( fut );
      cass_batch_free( batch );
    }

	
    rc_t
    IomCassandra::ReadObject( faodel::bucket_t bucket,
				   const kelpie::Key &key,
				   lunasa::DataObject *ldo )
    {
      rc_t krc = KELPIE_OK;
      CassError crc = CASS_OK;
      CassFuture* future = nullptr;
      CassStatement* stmt = nullptr;
      std::string select_cql = "SELECT bucket, key, type, meta_size, data_size, payload FROM " + keyspace_table_ + " WHERE bucket = ? AND key = ?";

      stmt = cass_statement_new( select_cql.c_str(), 2 );
      cass_statement_bind_string( stmt, 0, bucket.GetHex().c_str() );
      cass_statement_bind_string( stmt, 1, key.str().c_str() );

      future = cass_session_execute( session_, stmt );
      cass_future_wait( future );
      crc = cass_future_error_code( future );
      if( crc not_eq CASS_OK ) {
	std::cerr << "Cassandra read operation failed: " << future << std::endl;
      } else {
	cass_int64_t ds, ms;
	int8_t ldo_type;
	const cass_byte_t *buf;
	size_t buf_size;
	
	const CassResult* result = cass_future_get_result( future );
	CassIterator* iterator = cass_iterator_from_result( result );
	if( cass_iterator_next( iterator ) ) {
	  const CassRow* row = cass_iterator_get_row( iterator );
	  cass_value_get_int64( cass_row_get_column( row, 3 ), &ms );
	  cass_value_get_int64( cass_row_get_column( row, 4 ), &ds );
	  *ldo = lunasa::DataObject( ms, ds, lunasa::DataObject::AllocatorType::eager );
	  cass_value_get_bytes( cass_row_get_column( row, 5 ), &buf, &buf_size );
	  std::memcpy( ldo->GetMetaPtr(), buf, buf_size );
	  cass_value_get_int8( cass_row_get_column( row, 2 ), &ldo_type );
	  ldo->SetTypeID( ldo_type );
	  stat_rd_requests++;
	  stat_rd_bytes += buf_size + sizeof( int8_t ) + ( 2 * sizeof( uint64_t ) ) + bucket.GetHex().size() + key.str().size();
	} else {
	  krc = KELPIE_ENOENT;
	}

	cass_iterator_free( iterator );
	cass_result_free( result );
      }

      cass_future_free( future );
      cass_statement_free( stmt );
      return krc;
    }

    void
    IomCassandra::sstr( std::stringstream &ss, int depth, int index ) const
    {
      ss << std::string( index, ' ' ) + "IomCassandra cluster: " << cluster_endpoint_ << std::endl;
    }
    
    void
    IomCassandra::AppendWebInfo( faodel::ReplyStream rs,
				 const std::string reference_link,
				 const std::map< std::string, std::string > &args)
    {
      std::vector<std::vector<std::string> > items =
	{
	 {"Setting", "Value"},
	 {"Name",    name},
	 {"Cluster endpoint", cluster_endpoint_},
	};

      rs.mkTable(items, "Basic Information");
      rs.tableBegin("Initial Configuration Parameters");
      rs.tableTop({"Setting", "Value"});
      for(auto &&nvpair : settings) {
	rs.tableRow({nvpair.first, nvpair.second});
      }
      rs.tableEnd();

      auto ai = args.find("details");
      if(ai not_eq args.end() and ai->second == "true") {
	CassError rc = CASS_OK;
	CassStatement* stmt = nullptr;
	CassFuture* future = nullptr;
	
	auto ai2 = args.find("bucket");
	if(ai2 == args.end() or ai2->second.empty()) {
	  // we were not given a bucket, list them all
	  std::string select_cql = "SELECT bucket FROM " + keyspace_table_;

	  stmt = cass_statement_new( select_cql.c_str(), 0 );
	  future = cass_session_execute( session_, stmt );
	  cass_future_wait( future );
	  rc = cass_future_error_code( future );
	  if( rc not_eq CASS_OK ) {
	    std::cerr << "AppendWebInfo: Cassandra SELECT failed: " << future << std::endl;
	  } else {	    
	    // Get the list of buckets
	    std::vector<std::string> links;
	    const CassResult* result = cass_future_get_result( future );
	    CassIterator* iterator = cass_iterator_from_result( result );
	    while( cass_iterator_next( iterator ) ) {
	      const char* bucket_str;
	      size_t bucket_str_len;
	      
	      cass_value_get_string( cass_row_get_column( cass_iterator_get_row( iterator ), 0 ),
				     &bucket_str,
				     &bucket_str_len );
	      std::string t( bucket_str, bucket_str_len );
	      links.push_back("<a href=\"" +
			      reference_link +
			      "&details=true&iom_name=" +
			      name +
			      "&bucket=" + t +
			      "\">" + t +
			      "</a>");

	      rs.mkList(links, "On-disk buckets");
	    }
	  }
	} else {
	  // iterate the bucket
	  const std::string& bucket = ai2->second;
	  std::string select_cql = "SELECT * FROM " + keyspace_table_ + " WHERE bucket = " + bucket;
	  stmt = cass_statement_new( select_cql.c_str(), 0 );
	  future = cass_session_execute( session_, stmt );
	  cass_future_wait( future );
	  rc = cass_future_error_code( future );
	  if( rc not_eq CASS_OK ) {
	    std::cerr << "AppendWebInfo: Cassandra select failed: " << future << std::endl;
	  } else {
	    std::vector< std::pair < std::string, std::string > > blobs;
	    blobs.push_back( std::make_pair( "Key", "Size" ) );
	    const CassResult* result = cass_future_get_result( future );
	    CassIterator* iterator = cass_iterator_from_result( result );
							   
	    while( cass_iterator_next( iterator ) ) {
	      const char* key_str;
	      cass_int64_t ds, ms;
	      size_t key_str_len;
	      const CassRow* row = cass_iterator_get_row( iterator );
	      cass_value_get_string( cass_row_get_column( row, 2 ), &key_str, &key_str_len );
	      cass_value_get_int64( cass_row_get_column( row, 3 ), &ms );
	      cass_value_get_int64( cass_row_get_column( row, 4 ), &ds );

	      blobs.push_back( std::make_pair( std::string( key_str, key_str_len ), std::to_string( ms + ds ) ) );
	    }
      
	    rs.mkTable( blobs, "Objects in Bucket " + bucket );
	    cass_iterator_free( iterator );
	    cass_result_free( result );
	  }

	  cass_future_free( future );
	  cass_statement_free( stmt );
	}
      }
    }


    rc_t
    IomCassandra::GetInfo( faodel::bucket_t bucket, const kelpie::Key& key, object_info_t *info )
    {
      rc_t krc;
      CassError crc = CASS_OK;
      CassFuture* future = nullptr;
      CassStatement* stmt = nullptr;
      std::string select_cql = "SELECT meta_size, data_size FROM " + keyspace_table_ + " WHERE bucket = ? AND key = ?";

      // currently makes zero sense to call this with a null info pointer, so zero the struct
      // if the pointer is not null and bail out otherwise
      if( info not_eq nullptr ) {
	     info->Wipe();
      } else {
	     return KELPIE_EINVAL;
      }
      
      stmt = cass_statement_new( select_cql.c_str(), 2 );
      cass_statement_bind_string( stmt, 0, bucket.GetHex().c_str() );
      cass_statement_bind_string( stmt, 1, key.str().c_str() );
      future = cass_session_execute( session_, stmt );
      cass_future_wait( future );
      crc = cass_future_error_code( future );
      if( crc not_eq CASS_OK ) {
	std::cerr << "GetInfo: Cassandra select failed: " << future << std::endl;
      } else {
	const CassResult* result = cass_future_get_result( future );
	cass_int64_t ms, ds;
	
	if( cass_result_row_count not_eq 0 ) {
	  const CassRow* row = cass_result_first_row( result );
	  cass_value_get_int64( cass_row_get_column( row, 0 ), &ms );
	  cass_value_get_int64( cass_row_get_column( row, 1 ), &ds );
	  info->col_user_bytes = ms + ds;
	  info->col_availability = kelpie::Availability::InDisk;
	  krc = KELPIE_OK;	  
	} else {
	  info->col_availability = kelpie::Availability::Unavailable;
	  krc = KELPIE_ENOENT;
	}

	cass_result_free( result );
      }

      cass_statement_free( stmt );
      cass_future_free( future );

      return krc;
    }


  }
}

	
      
			 
      

      
      

      

      
	
