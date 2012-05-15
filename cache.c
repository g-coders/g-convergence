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
      fprintf(stderr, "Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
      return NULL;
    }
 
  if (mysql_real_connect(conn, "localhost", "dluhosma", "emir6:fe", 
                         NULL, 0, NULL, 0) == NULL)
    {
      fprintf(stderr, "Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
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

/**
 * Determines whether given url can be safely inserted.
 * @return 1 if it is safe, 0 otherwise.
 */
int is_url_safe(char *url)
{
  /* STUB */
  return 1;
} // is_url_safe

/**
 * Invoke error functions specific to prepared statements.
 * @param stmt, a statement handler
 * @param message, the error message
 */
static void
print_stmt_error (MYSQL_STMT *stmt, char *message)
{
  fprintf (stderr, "%s\n", message);
  if (stmt != NULL)
  {
    fprintf (stderr, "Error %u (%s): %s\n",
             mysql_stmt_errno (stmt),
             mysql_stmt_sqlstate (stmt),
             mysql_stmt_error (stmt));
  }
} // print_stmt_error

/**
 * Determines whether given url can be safely inserted.
 * @param *conn, a connection struct
 * @param stmt_str, a mysql query to execute
 * @return 1 if it is safe, 0 otherwise.
 */ 
int execute_query(MYSQL *conn, const char *stmt_str) 
{
  MYSQL_STMT *stmt;

  /* Declare bind variables. */
  MYSQL_BIND param[1]; // IS THIS RIGHT?
  
  /* Allocate statement handler. */
  stmt = mysql_stmt_init(conn);

  if (stmt == NULL)
    {
      print_stmt_error(stmt, "Could not initialize statement handler.");
      return 0;
    }

  /* Send the statement to the server to be prepared. */
  if (mysql_stmt_prepare(stmt, stmt_str, strlen(stmt_str)) != 0)
    {
      print_stmt_error(stmt, "Could not prepare statement.");
      return 0;
    }

  /* zero the structures */
  memset ((void *) param, 0, sizeof (param)); 

  /* SET THE PARAMS!!! */
  
  /* Bind structures to the statement. */
  if (mysql_stmt_bind_param(stmt, param) != 0)
    {
      print_stmt_error(stmt, "Could not bind parameters.");
      return 0;
    }

  /* Execute the statement. */
  if (mysql_stmt_execute(stmt) != 0)
    {
      print_stmt_error(stmt, "Could not execute statement.");
      return 0;
    }
  
  /* Deallocate statement handler. */
  mysql_stmt_close(stmt);
  return 1;
} // execute_parametrized_query

/* Inserts a certificate fingerprint into the cache.
 * Inserts into trusted cache is db is set to CACHE_TRUSTED;
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

  /* Execute the query. */
  if ((return_val = mysql_query(conn, query_string)))
  {
    printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
  }

  free(db_name);
  free(query_string);
  close_mysql_connection(conn);

  return !return_val;
} //cache_insert

/* Remove a specific url from the cache. 
 * Removes from trusted cache if db is set to true;
 * otherwise, removes from blacklist cache.
 * Returns 1 if removal is successful, 0 if url cannot be removed,
 * and -1 if the url does not exist in the database. 
 */
int cache_remove (char* url, int db)
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

  asprintf(&query_string, "DELETE FROM %s WHERE url=%s",
           db_name, url);

  /* Execute the query. */
  if ((return_val = mysql_query(conn, query_string)))
  {
    printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
  }

  free(db_name);
  free(query_string);
  close_mysql_connection(conn);

  return !return_val;
} //cache_remove

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
  asprintf (&query_string, "SELECT url FROM blacklisted WHERE url=%s", url);

  /* Execute the query. */
  if (mysql_query(conn, query_string))
  {
    printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
    free(query_string);
    close_mysql_connection(conn);
    return -1;
  }
  
  result = mysql_store_result(conn);
  num_elements = mysql_num_rows(result);

  free(query_string);
  close_mysql_connection(conn);
   
  return num_elements;
} //is_blacklisted

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
            "SELECT url FROM trusted WHERE fingerprint=%s",
            fingerprint);

  /* Execute the query. */
  if (mysql_query(conn, query_string))
  {
    printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
    free(query_string);
    close_mysql_connection(conn);
    return -1;
  }
  
  result = mysql_store_result(conn);
  num_rows = mysql_num_rows(result);

  free(query_string);
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


/**
 * Removes all entries from trusted on a cache miss and add the ones newly
 * retrieved from the url
 * @return 1 on success, 0 on failure
 */
int cache_update_url (char *url, char **fingerprints, int num_of_certs)
{
  MYSQL *conn = start_mysql_connection();
  int return_value1;
  int return_value2 = 0;
  int temp_value2; // Flag to tell whether any of the cache inserts failed
  int i; // iteration variable
  
  return_value1 = cache_remove(url, CACHE_TRUSTED);

  for(i=0; i<num_of_certs; i++)
    {
      temp_value2 = cache_insert(url, fingerprints[i], CACHE_TRUSTED);
      if(!temp_value2) // If the operation failed
        return_value2 = 1; // Set return_value2 to failure
    }

  close_mysql_connection(conn);

  return !(return_value1 || return_value2);
} //cache_update
