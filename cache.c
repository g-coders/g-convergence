/******************************************************************************
 * authors: g-coders
 * created: February 21, 2012
 * revised: May 5, 2012
 * Description: This file contains functions which are responsible for
 * managing the cache of verified websites.
 ******************************************************************************/
#include "cache.h"

/* Structure of table
 *   trusted
 *   +--------------------------------------------------------------+
 *   | id  | url                  | fingerprint      |  timestamp   | 
 *   +--------------------------------------------------------------+
 *   | 1   | https://facebook.com | BL:AH:BL:AH: ..  | TIME         |
 *   | 2   | https://chase.com    | ME:OW:WO:OF: ..  | TIME         |
 *   |   ....                                                       |
 *   +--------------------------------------------------------------+
 * */

/* TODO
 * implement url_safe
 * need to do some serious refactoring- the following pattern repeats:
     connect to db
     construct a query
     execute the query on the db and store the result
     close connection to db
     interpret the result
 * examine all SQL queries and fix their syntax
 */

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Helpers
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/* Opens an SQL conenction, and returns a pointer to it. Exits and returns 
 * an error if connection cannot be established.
 * @returns a mysql connection pointer
 */
MYSQL* 
start_mysql_connection()
{
  //Create a new MYSQL connection struct and set it to the result of the 
  //connection call.
  MYSQL *conn;
  conn = mysql_init(NULL);

  //If the connection wasn't made correctly, return an error
  if (!conn) 
    {
      printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
      return NULL;
    }
 
  if (mysql_real_connect(conn, "localhost", "zetcode", "passwd", 
                         NULL, 0, NULL, 0) == NULL)
    {
      printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
      exit(1);
    }

  return conn;
} //start_mysql_connection

/* Closes the db connection.
 * @param connection a pointer to MYSQL connection
 */ 
void 
close_mysql_connection(MYSQL *connection)
{
  mysql_close(connection);
} // close_mysql_connection
  
/* Determines whether given fingerprint can be safely inserted.
 * @return 1 if it is safe, 0 otherwise
 */
int
is_fingerprint_safe(char *fingerprint)
{
  return verify_fingerprint_format(fingerprint);
} // is_fingerprint_safe

/* Determines whether given url can be safely inserted.
 * @return 1 if it is safe, 0 otherwise.
 */ 
int is_url_safe(char *url) 
{
  return 0;
  /* STUB */
} // is_url_safe

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Cache functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/**
 * Checks if a particular fingerprint is in the cache already, either as
 * the fingerprint of a trusted url or a blacklisted url.  Returns
 * CACHE_TRUSTED if the cache has a fingerprint for the url,
 * CACHE_BLACKLIST if the blacklist has the fingerprint, 0 if the url is
 * not in either cache, and -1 if an error is encountered.
 */
int is_in_cache (char *url, char *fingerprint)
{
  MYSQL_RES *result;
  MYSQL *conn;
  int num_rows;
  int blacklisted;
  char *query_string = NULL;

  /* First, check if url is blacklisted. */
  conn = start_mysql_connection();
  blacklisted = is_blacklisted(url);
  if (blacklisted)
    {
      return CACHE_BLACKLIST;
    }

  asprintf (&query_string, 
            "SELECT url FROM trusted WHERE fingerprint=%s;",
            fingerprint);

  mysql_query(conn, query_string);
  result = mysql_store_result(conn);
  num_rows = mysql_num_rows(result);

  close_mysql_connection(conn);
  
  if (num_rows)
    {
      return CACHE_TRUSTED;
    }
  else 
    {
      return CACHE_BLACKLIST;
    }
} //is_in_cache

/* Checks if we have a record of a url in the blacklist. 
 * @return 1 if the url is in the blacklist, 0 if it is not, 
 * and -1 if error is encountered.
 */
int 
is_blacklisted (char *url)
{
  MYSQL *conn = start_mysql_connection();

  MYSQL_RES *result;
  unsigned int num_elements;

  char *query_string = NULL;
  asprintf (&query_string, "SELECT url FROM blacklisted WHERE url=%s;", url);

  mysql_query(conn, query_string);
  result = mysql_store_result(conn);
  num_elements = mysql_num_rows(result);

  close_mysql_connection(conn);
   
  return num_elements;
} //is_blacklisted

/* Inserts a certificate fingerprint into the cache.
 * Inserts into trusted cache is db is set to true;
 * otherwise, inserts into blacklist cache.
 * Returns 1 if insert is successful. Otherwise, returns 0.
 */ 
int cache_insert (char* url, char* fingerprint, int db)
{
  char *query_string = NULL;
  char *db_name = NULL;
  char *timestamp;
  int return_val;
  MYSQL *conn = start_mysql_connection();
 
  if (db == CACHE_TRUSTED)
    {
      asprintf(&db_name, "trusted");
    }
  else
    {
      asprintf(&db_name, "blacklisted");
    }

  /* Compute timestamp. */
  /* Do we want time inserted or time at which it will expire? */

  /* Construct the query string. */
  asprintf(&query_string, "INSERT INTO %s VALUES (%s, %s, %s)",
           db_name, url, fingerprint, timestamp);

  return_val = mysql_query(conn, query_string);
  close_mysql_connection(conn);

  return !return_val;
} //cache_insert

/* Remove a specific certificate fingerprint from the cache. 
 * Removes from trusted cache if db is set to true;
 * otherwise, removes from blacklist cache.
 * Returns 1 if removal is successful, 0 if fingerprint cannot be removed,
 * and -1 if the fingerprint does not exist in the database. 
 */
int cache_remove (char* fingerprint, int db)
{
  int return_val;
  char *db_name;
  char *query_string = NULL;
  MYSQL *conn = start_mysql_connection();
 
  if (db == CACHE_TRUSTED)
    {
      asprintf(&db_name, "trusted");
    }
  else
    {
      asprintf(&db_name, "blacklisted");
    }

  asprintf(&query_string, "DELETE FROM %s WHERE fingerprint=%s;",
           db_name, fingerprint);
  return_val = mysql_query(conn, query_string); 
  close_mysql_connection(conn);

  return !return_val;
} //cache_remove

/* Removes cache entries that have expired from trusted cache. 
 * @return 1 on success, 0 on failure
 */
int cache_update_url (char *url, char *fingerprints)
{
  MYSQL *conn = start_mysql_connection();
  int return_value1, return_value2;
  
  return_value1 =
    mysql_query(conn, "DELETE FROM trusted(fingerprint)");
  return_value2 = mysql_query(conn, "SELECT * FROM trusted WHERE timestamp < ");

  close_mysql_connection(conn);

  return !(return_value1 || return_value2);
} //cache_update
