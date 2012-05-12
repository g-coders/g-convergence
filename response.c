/** @file
 
  @brief  functions which format the verification response and functions which send the response back to the client.
 
  @author
  Tolu Alabi, Zach Butler, Martin Dluhos
 
  @date
  Created: February 21, 2012 <br />
  Last revised: March 7, 2012
*/

#include "response.h"
#include "certificate.h"
#include "cache.h"
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/pem.h>

//WE ARE NOT SURE ABOUT THIS
#define RESPONSE_LEN 201
#define MAX_NO_OF_CERTS 7

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Helpers
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/** 
 @brief allocates and fills the answer string field in connection struct. 
 
 @param connection_info  FIXME
 @param answer_string    FIXME
 */
void
set_answer_string(struct connection_info_struct* connection_info, char* answer_string)
{  
  connection_info->answer_string = malloc(sizeof(char) * (strlen(answer_string) + 1) );
  strcpy((char*)connection_info->answer_string, answer_string);
}

/** 
  @brief Generates a signature of a fingerprint list.
 
  @param fingerprint_list  list of fingerprints
  @param signature         string to hold the digital signature of fingerprints
  @param signature_size    FIXME
  @param private_key       FIXME

  @return 1 on success, 0 on otherwise FIXME does RSA_sign return an error if it fails?
 */
int
generate_signature(unsigned char *fingerprint_list, unsigned char *signature,
                   unsigned int *signature_size, RSA *private_key)
{
  unsigned char *return_val;
  /* Digest generated by SHA-1. */
  unsigned char digest[SHA_DIGEST_LENGTH];
  
  /* Generate a SHA-1 digest of the fingerprint list. */
  return_val = SHA1(fingerprint_list, SHA_DIGEST_LENGTH, digest);
  
  if (return_val == NULL)
    return 0;
 
  /* Generate a signature of the digest using the notary's private key. */
  return RSA_sign(NID_sha1, digest, SHA_DIGEST_LENGTH, signature,
                  signature_size, private_key);
} // generate_signature

/** 
  @brief Obtains a response to a POST/GET request.
 
  @param coninfo_cls             FIXME
  @param host_to_verify          FIXME
  @param fingerprint_from_client FIXME

  @return MHD_YES FIXME (209...??) if __, MHD_NO otherwise. 
 */
int
retrieve_response (void *coninfo_cls, host *host_to_verify, const char *fingerprint_from_client)
{
  int verified, num_of_certs; // was certificate verified?
  char *fingerprints_from_website[MAX_NO_OF_CERTS];
  char *json_fingerprint_list; // the response to send to client
  char *fingerprint_to_send; // which fingerprint will we send to the user?
  unsigned int *signature_size = NULL;
  unsigned char *signature;
  RSA *private_key;
  FILE *key_file = fopen(key_filename, "r");

  struct connection_info_struct *con_info = coninfo_cls;

  //variables to store the time stamp
  size_t start_time, end_time;
  start_time = time(NULL);
  // Check if the url is in the cache
  int cache_val;
  if(cache_val = is_in_cache(host_to_verify->url, fingerprint_from_client))
    {
      if(cache_val == CACHE_BLACKLIST)
        {
          con_info->answer_code = MHD_HTTP_CONFLICT;
        }
      else // We have a trusted website
        {
          con_info->answer_code = MHD_HTTP_OK;
        }
      fingerprint_to_send = fingerprint_from_client;
    }
  else {  
    //create space for the fingerprints
    int i;
    for(i=0; i<MAX_NO_OF_CERTS; i++)
      fingerprints_from_website[i] = calloc(FPT_LENGTH, sizeof(char));

    //get the fingerprints
    num_of_certs =
      request_certificate(host_to_verify, fingerprints_from_website);

    if (num_of_certs == 0)
      {
        /* The notary could not obtain the certificate from the website
         * for some reason.
         */
        con_info->answer_code = MHD_HTTP_SERVICE_UNAVAILABLE; //503
        return MHD_NO;
      } // if

    if (fingerprint_from_client != NULL)
      {
        verified = verify_certificate
          (fingerprint_from_client, fingerprints_from_website, num_of_certs);

        if (verified == 1)
          con_info->answer_code = MHD_HTTP_OK; // 200
        else
          {
            con_info->answer_code = MHD_HTTP_CONFLICT; // 409
          }
      }
    else con_info->answer_code = MHD_HTTP_OK;

    fingerprint_to_send = fingerprints_from_website[0];

    // Update cache
    int cache_return = cache_update_url(host_to_verify->url,
                                        fingerprints_from_website,
                                        num_of_certs);
    if(!cache_return)
      fprintf(stderr, "Cache didn't update properly.\n");

    // free memory used for fingerprints
    for(i=0; i<MAX_NO_OF_CERTS; i++)
    free(fingerprints_from_website[i]);
  }

  /* Format the response which will be sent to client.
   * Note that this response is sent both on a successful verification
   * and on a failed verification.
   * The JSON format of the response is available at
   * https://github.com/moxie0/Convergence/wiki/Notary-Protocol
   */
  //get end_time for processing the request
  end_time = time(NULL);

  json_fingerprint_list = malloc ( (RESPONSE_LEN + 1) * sizeof (char));
  sprintf (json_fingerprint_list,
           "{\n \
\t\"fingerprintList\":\n \
\t[\n \
\t {\n \
\t \"timestamp\": {\"start\": \"%Zu\", \"finish\": \"%Zu\"},\n \
\t \"fingerprint\": \"%s\"\n \
\t }\n \
\t]\n\
}\n", start_time, end_time, fingerprint_to_send);

   /* Get the RSA private key from a file. */
  //private_key = PEM_read_RSAPrivateKey(key_file, NULL, NULL, NULL);

  /* Calculate the signature size and allocate space for it. */
  //*signature_size = RSA_size(private_key);
  //signature = (unsigned char *) malloc (*signature_size);

  //generate_signature((unsigned char *) json_fingerprint_list, signature, signature_size, private_key);

  set_answer_string(con_info, json_fingerprint_list);
  
  free(json_fingerprint_list);

  return MHD_YES;
}


/** 
 @brief Sends response back to the client. 

 @param connection     X
 @param response_data  X
 @param status_code    X

 @return the response code (200, 409, etc.), or MHD_NO if unable to send a response. 
 */
int
send_response (struct MHD_Connection *connection, const char *response_data,
               int status_code)
{
  int return_value;
  struct MHD_Response *response;

  response = MHD_create_response_from_data (strlen(response_data), (void*) response_data, MHD_NO, MHD_NO);

  if (response == NULL)
  {
    return MHD_NO;
  }

  return_value = MHD_queue_response (connection, status_code, response);
  MHD_destroy_response (response);

  return return_value;
}
