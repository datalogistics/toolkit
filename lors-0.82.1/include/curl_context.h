/*!
 * @file curl_context.h
 * @brief Description goes here
 *
 * @author Ezra Kissel ezkissel@indiana.edu
 * @date
 * @version
 * @details curl is a command line tool for transferring data with URL syntax
 *
 */
#ifndef _GNU_SOURCE
/*! @def _GNU_SOURCE
    access to extra non-posix GNU stuff.
 */
#define _GNU_SOURCE
#endif

#ifndef CURL_CONTEXT_H
/*! @def CURL_CONTEXT_H
    Make sure not to include this file more than once.
 */
#define CURL_CONTEXT_H

#include <curl/curl.h>
#include <th-lock.h>

/*!
 * @brief struct curl_context_t
 *
 * @details [detailed description]
 */
typedef struct curl_context_t {
	CURL *curl;       /**< CURL pointer */
	char *url;        /**< char pointer to URL */
	
	int curl_persist; /**< this integer is used for */
	int use_ssl;      /**< are we using SSL? */
	int use_cookies;  /**< are we using cookies? */
	int follow_redirect; /**< should we follow 302? */

	char *certfile;   /**< char pointer to certfile */
	char *keyfile;    /**< char pointer to keyfile */
	char *keypass;    /**< char pointer to keypass */
	char *cacerts;    /**< char pointer to cacerts */
	char *cookiejar;  /**< char pointer to cookie jar */
} curl_context;

typedef struct curl_response_t {
	char *data;
	char *redirect_url;
	char *content_type;
	long status;
} curl_response;

/*!
 * @brief struct curl_http_data
 *
 * @sa [see also section]
 * @note [any note about the function you might have]
 * @warning [any warning if necessary]
 *
 * @details [detailed description]
 *
 */
struct curl_http_data {
        char *ptr;       /**< char pointer to */
        char *lptr;      /**< char pointer to */
        long len;        /**< length */
};

/*!
 * @brief 
 *
 * @fn init_curl(curl_context *cc, long flags)
 * @param cc
 * @param flags
 * @return Returns an integer.
 * @sa [see also section]
 * @note [any note about the function you might have]
 * @warning [any warning if necessary]
 *
 * @details [detailed description]
 */
int init_curl(curl_context *cc, long flags);

curl_response *init_curl_response();

int free_curl_response(curl_response *cr);

/*!
 * @brief 
 *
 * @fn copy_curl_context(curl_context *src, curl_context *dst)
 * @param src
 * @param dst
 * @return Returns an integer.
 *
 * @details [detailed description]
 */
int copy_curl_context(curl_context *src, curl_context *dst);

char *curl_post(curl_context *cc, char *target, char *accept_type, char *content_type,
		    char *post_fields, char *post_data, curl_response **ret_data);

char *curl_get(curl_context *cc, char *target, char *accept_type, curl_response **ret_data);

/*!
 * @brief 
 *
 * @fn curl_post_json_string(curl_context *cc, char *target, char *send_str, curl_response *ret_data)
 * @param cc
 * @param target
 * @param send_str
 * @param ret_str
 * @return Returns a curl response struct
 * 
 * @details 
 */
char *curl_post_json_string(curl_context *cc, char *target, char *send_str, curl_response **ret_data);

/*!
 * @brief 
 *
 * @fn curl_get_json_string(curl_context *cc, char *target, char *send_str, curl_response *ret_data)
 * @param cc
 * @param target
 * @param ret_str
 * @return Returns a curl response struct
 * 
 * @details 
 */
char *curl_get_json_string(curl_context *cc, char *target, curl_response **ret_data);

#endif // CURL_CONTEXT_H
