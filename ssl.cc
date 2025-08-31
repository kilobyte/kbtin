#include "tintin.h"
#include <sys/stat.h>
#include "protos/print.h"
#include "protos/utils.h"

#ifdef HAVE_GNUTLS
static gnutls_certificate_credentials_t ssl_cred=0;
static int ssl_check_cert(gnutls_session_t sslses, const char *host, int validate, struct session *oldses);

#define FAIL(...) do{tintin_eprintf(oldses, "#Error: " __VA_ARGS__, gnutls_strerror(ret)); gnutls_deinit(sslses); return 0; }while(0)

enum {VAL_NONE, VAL_CA, VAL_SAVE};

gnutls_session_t ssl_negotiate(int sock, const char *host, const char *extra, struct session *oldses)
{
    gnutls_session_t sslses;
    int ret;
    int validate;

    if (!*extra || !strcmp(extra, "ca") || !strcmp(extra, "CA"))
        validate = VAL_CA;
    else if (!strcmp(extra, "none"))
        validate = VAL_NONE;
    else if (!strcmp(extra, "save"))
        validate = VAL_SAVE;
    else
    {
        tintin_eprintf(oldses, "#Error: unknown security model '%s'", extra);
        return 0;
    }

    if (!ssl_cred)
    {
        gnutls_global_init();
        if ((ret = gnutls_certificate_allocate_credentials(&ssl_cred)))
            die("#Error: gnutls: %s", gnutls_strerror(ret));
        if ((ret = gnutls_certificate_set_x509_system_trust(ssl_cred)) < 0)
            tintin_eprintf(oldses, "#Warning: can't find system CAs: %s", gnutls_strerror(ret));
    }

    if ((ret = gnutls_init(&sslses, GNUTLS_CLIENT)))
    {
        tintin_eprintf(oldses, "#Error: gnutls_init: %s", gnutls_strerror(ret));
        return 0;
    }
    if ((ret = gnutls_set_default_priority(sslses)))
        FAIL("gnutls_set_default_priority: %s");
    if ((ret = gnutls_credentials_set(sslses, GNUTLS_CRD_CERTIFICATE, ssl_cred)))
        FAIL("gnutls_credentials_set: %s");
    if ((ret = gnutls_server_name_set(sslses, GNUTLS_NAME_DNS, host, strlen(host))))
        FAIL("gnutls_server_name_set(%s): %s", host);
    if (validate == VAL_CA)
        gnutls_session_set_verify_cert(sslses, host, 0);
    gnutls_transport_set_int(sslses, sock);
    do
    {
        ret=gnutls_handshake(sslses);
    } while (ret==GNUTLS_E_AGAIN || ret==GNUTLS_E_INTERRUPTED);
    if (ret)
    {
        tintin_eprintf(oldses, "#SSL handshake failed: %s", gnutls_strerror(ret));
        if (ret == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR)
        {
            ret = gnutls_session_get_verify_cert_status(sslses);
            gnutls_certificate_type_t cert_type = gnutls_certificate_type_get(sslses);

            gnutls_datum_t err = {0};
            gnutls_certificate_verification_status_print(ret, cert_type, &err, 0);
            tintin_eprintf(oldses, "#%s", err.data);
            gnutls_free(err.data);
        }
        gnutls_deinit(sslses);
        return 0;
    }
    if (validate!=VAL_CA && !ssl_check_cert(sslses, host, validate, oldses))
    {
        gnutls_deinit(sslses);
        return 0;
    }
    return sslses;
}


static bool cert_file(const char *name, char *respath)
{
    char fname[BUFFER_SIZE], *fn;
    const char *home;

    if (!*name || *name=='.')   // no valid hostname starts with a dot
        return false;
    fn=fname;
    while (1)
    {
        if (!*name)
            break;
#ifdef WIN32
        else if (*name==':')
        {
            name++;
            *fn++='.';
        }
#endif
        else if (is7alnum(*name) || *name=='-' || *name=='.' || *name=='_')
            *fn++=*name++;
        else
            return false;
    }
    if (*(fn-1)=='.')   // no valid hostname ends with a dot, either
        return false;
    *fn=0;
    if (!(home=getenv("HOME")))
        home=".";
    snprintf(respath, BUFFER_SIZE, "%s/%s/%s/%.253s.crt", home, CONFIG_DIR, CERT_DIR, fname);
    return true;
}


static void load_cert(gnutls_x509_crt_t *cert, const char *name)
{
#   define BIGBUFSIZE 65536
    char buf[BIGBUFSIZE];
    FILE *f;
    gnutls_datum_t bptr;

    if (!cert_file(name, buf))
        return;
    if (!(f=fopen(buf, "r")))
        return;
    bptr.size=fread(buf, 1, BIGBUFSIZE, f);
    bptr.data=(unsigned char*)buf;
    fclose(f);

    gnutls_x509_crt_init(cert);
    if (gnutls_x509_crt_import(*cert, &bptr, GNUTLS_X509_FMT_PEM))
    {
        gnutls_x509_crt_deinit(*cert);
        *cert=0;
    }
}


static void save_cert(gnutls_x509_crt_t cert, const char *name, bool first, struct session *oldses)
{
    char fname[BUFFER_SIZE], buf[BIGBUFSIZE];
    const char *home;
    FILE *f;
    size_t len;

    len=BIGBUFSIZE;
    if (gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_PEM, buf, &len))
        return;
    if (!(home=getenv("HOME")))
        home=".";
    snprintf(fname, BUFFER_SIZE, "%s/%s", home, CONFIG_DIR);
    if (mkdir(fname, 0777) && errno!=EEXIST)
    {
        tintin_eprintf(oldses, "#Cannot create config dir (%s): %s", fname, strerror(errno));
        return;
    }
    snprintf(fname, BUFFER_SIZE, "%s/%s/%s", home, CONFIG_DIR, CERT_DIR);
    if (mkdir(fname, 0755) && errno!=EEXIST)
    {
        tintin_eprintf(oldses, "#Cannot create certs dir (%s): %s", fname, strerror(errno));
        return;
    }
    if (!cert_file(name, fname))
        return;
    if (first)
        tintin_printf(oldses, "#It is the first time you connect to this server.");
    tintin_printf(oldses, "#Saving server certificate to %s", fname);
    if (!(f=fopen(fname, "w")))
    {
        tintin_eprintf(oldses, "#Save failed: %s", strerror(errno));
        return;
    }
    if (fwrite(buf, 1, len, f)!=len)
    {
        tintin_eprintf(oldses, "#Save failed: %s", strerror(errno));
        fclose(f);
        unlink(fname);
        return;
    }
    if (fclose(f))
    {
        tintin_eprintf(oldses, "#Save failed: %s", strerror(errno));
        unlink(fname);
    }
}


static bool diff_certs(gnutls_x509_crt_t c1, gnutls_x509_crt_t c2)
{
    char buf1[BIGBUFSIZE], buf2[BIGBUFSIZE];
    size_t len1, len2;

    len1=len2=BIGBUFSIZE;
    if (gnutls_x509_crt_export(c1, GNUTLS_X509_FMT_DER, buf1, &len1))
        return true;
    if (gnutls_x509_crt_export(c2, GNUTLS_X509_FMT_DER, buf2, &len2))
        return true;
    if (len1!=len2)
        return true;
    return memcmp(buf1, buf2, len1);
}


static int ssl_check_cert(gnutls_session_t sslses, const char *host, int validate, struct session *oldses)
{
    char fname[BUFFER_SIZE], buf2[BUFFER_SIZE], *bptr;
    time_t t;
    gnutls_x509_crt_t cert, oldcert;
    const gnutls_datum_t *cert_list;
    unsigned int cert_list_size;
    const char *err=0;

    oldcert=0;
    if (validate == VAL_SAVE)
        load_cert(&oldcert, host);

    if (gnutls_certificate_type_get(sslses)!=GNUTLS_CRT_X509)
    {
        err="server doesn't use x509 -> no key retention.";
        goto nocert;
    }

    if (!(cert_list = gnutls_certificate_get_peers(sslses, &cert_list_size)))
    {
        err="server has no x509 certificate -> no key retention.";
        goto nocert;
    }

    gnutls_x509_crt_init(&cert);
    if (gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER)<0)
    {
        err="server's certificate is invalid.";
        goto badcert;
    }

    t=time(0);
    if (gnutls_x509_crt_get_activation_time(cert)>t)
    {
        ctime_r(&t, buf2);
        if ((bptr=strchr(buf2, '\n')))
            *bptr=0;
        snprintf(fname, BUFFER_SIZE, "certificate activation time is in the future (%.128s).",
            buf2);
        err=fname;
    }

    if (gnutls_x509_crt_get_expiration_time(cert)<t)
    {
        ctime_r(&t, buf2);
        if ((bptr=strchr(buf2, '\n')))
            *bptr=0;
        snprintf(fname, BUFFER_SIZE, "certificate has expired (on %.128s).",
            buf2);
        err=fname;
    }

    if (validate == VAL_NONE)
        goto badcert;

    if (!oldcert)
        save_cert(cert, host, 1, oldses);
    else if (diff_certs(cert, oldcert))
    {
        t-=gnutls_x509_crt_get_expiration_time(oldcert);
        if (err)
        {
            snprintf(buf2, BUFFER_SIZE, "certificate mismatch, and new %.128s",
                     err);
            err=buf2;
        }
        else if (t<-7*24*3600)
            err = "the server certificate is different from the saved one.";
        else
        {
            tintin_printf(oldses, (t>0)?
                "#SSL notice: server certificate has changed, but the old one was expired.":
                "#SSL notice: server certificate has changed, but the old one was about to expire.");
            /* Replace the old cert */
            save_cert(cert, host, 0, oldses);
            gnutls_x509_crt_deinit(oldcert);
            oldcert=0;
        }
    }
    else
    {
        /* All ok */
        gnutls_x509_crt_deinit(oldcert);
        oldcert=0;
    }


badcert:
    gnutls_x509_crt_deinit(cert);

nocert:
    if (oldcert)
        gnutls_x509_crt_deinit(oldcert);
    if (!err)
        return 1;

    switch (validate)
    {
#if 0
    case VAL_CA:
        tintin_eprintf(oldses, "#SSL error: %s", err);
        tintin_eprintf(oldses, "############################################################");
        tintin_eprintf(oldses, "##################### SECURITY ALERT #######################");
        tintin_eprintf(oldses, "############################################################");
        tintin_eprintf(oldses, "# SSL failed verification.  Either your connection is      #");
        tintin_eprintf(oldses, "# being intercepted, your system is screwed up, or the     #");
        tintin_eprintf(oldses, "# server's admin made an error.  In any case, something    #");
        tintin_eprintf(oldses, "# is fishy.  Aborting connection.                          #");
        tintin_eprintf(oldses, "############################################################");
        tintin_eprintf(oldses, "# NOTE: with MUD server security being as bad as it is,    #");
        tintin_eprintf(oldses, "# you may want to revert to old KBtin's security model by  #");
        tintin_eprintf(oldses, "# appending 'save' or forego all checks with 'none'.       #");
        tintin_eprintf(oldses, "############################################################");
    return 0;
#endif

    case VAL_SAVE:
        if (!oldcert)
        {
            tintin_printf(oldses, "#SSL warning: %s", err);
            tintin_printf(oldses, "#You may be vulnerable to Man-in-the-Middle attacks.");
            return 2;
        }

        tintin_eprintf(oldses, "#SSL error: %s", err);
        tintin_eprintf(oldses, "############################################################");
        tintin_eprintf(oldses, "##################### SECURITY ALERT #######################");
        tintin_eprintf(oldses, "############################################################");
        tintin_eprintf(oldses, "# It's likely you're under a Man-in-the-Middle attack.     #");
        tintin_eprintf(oldses, "# Someone may be trying to intercept your connection.      #");
        tintin_eprintf(oldses, "#                                                          #");
        tintin_eprintf(oldses, "# Another explanation is that the server's settings were   #");
        tintin_eprintf(oldses, "# changed in an unexpected way.  You can choose to avoid   #");
        tintin_eprintf(oldses, "# connecting, investigate this or blindly trust that the   #");
        tintin_eprintf(oldses, "# server is kosher.  To continue, please delete the file:  #");
        if (cert_file(host, fname))
            tintin_eprintf(oldses, "# %-57s#", fname);
        else
            {} /* can't happen */
        tintin_eprintf(oldses, "############################################################");
        tintin_eprintf(oldses, "#Aborting connection!");
        return 0;

    case VAL_NONE:
        tintin_printf(oldses, "#SSL warning: %s", err);
        return 2;

    default:
        abort();
    }
}

#endif
