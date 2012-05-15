/**
 * @file blacklist.c
 * 
 * @brief Allows the notary administrator to insert urls into blacklisted cache or
 * remove urls from the blacklisted cache.
 *
 * @author 
 * Tolu Alabi, Zach Butler, Martin Dluhos, Chase Felker, Radhika Krishnar
 */
#include "cache.h"

static void print_usage()
{
  printf("usage: blacklist <operation> url\n");
} // print_usage


int main(int argc, char *argv[])
{
  char *query_string = NULL;
  int return_val;
  MYSQL *conn = start_mysql_connection();

  /* Check if correct number of arguments supplied. */
  if (argc != 3)
  {
    fprintf(stderr, "Incorrect number of arguments!\n");
    print_usage();
    return 1;
  }

  /* Check if url has the correct format. */
  if (!is_url_safe(argv[2]))
  {
    fprintf(stderr, "Incorrect url given as third argument: %s\n", argv[2]);
    print_usage();
    return 1;
  }

  /* Process arguments from the command line and construct the query. */
  if (strcmp(argv[1], "insert") == 0)
  {
    asprintf(&query_string, "INSERT INTO blacklisted where url=%s", argv[2]);
  }
  else if (strcmp(argv[1], "remove") == 0)
  {
    asprintf(&query_string, "DELETE FROM blacklisted where url=%s", argv[2]);
  }
  else
  {
    fprintf(stderr, "Incorrect operation given as second argument: \
	%s!\n", argv[1]);
    print_usage();
    return 1;
  }

  /* Execute the query. */
  if (return_val = mysql_query(conn, query_string))
  {
    printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
  }

  /* Cleanup. */
  free(query_string);
  close_mysql_connection(conn);

  return return_val;
} // main
