#include "qrzapi.h"
#include "../common/http.h"

#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ham_qrzapi_init(ham_qrzapi *a, const char *user, const char *pwd) {
    memset(a, 0, sizeof(*a));
    if (user) a->username = strdup(user);
    if (pwd)  a->password = strdup(pwd);
}

void ham_qrzapi_free(ham_qrzapi *a) {
    if (!a) return;
    free(a->session_key);
    free(a->username);
    free(a->password);
    memset(a, 0, sizeof(*a));
}

static char *find_child_text(xmlNode *parent, const char *name) {
    for (xmlNode *n = parent->children; n; n = n->next) {
        if (n->type == XML_ELEMENT_NODE &&
            strcasecmp((const char *)n->name, name) == 0) {
            xmlChar *t = xmlNodeGetContent(n);
            char *ret = t ? strdup((const char *)t) : NULL;
            if (t) xmlFree(t);
            return ret;
        }
    }
    return NULL;
}

static xmlNode *find_child(xmlNode *parent, const char *name) {
    for (xmlNode *n = parent->children; n; n = n->next) {
        if (n->type == XML_ELEMENT_NODE &&
            strcasecmp((const char *)n->name, name) == 0) return n;
    }
    return NULL;
}

static int do_login(ham_qrzapi *a) {
    char *eu = ham_url_encode(a->username ? a->username : "");
    char *ep = ham_url_encode(a->password ? a->password : "");
    if (!eu || !ep) { free(eu); free(ep); return -1; }

    char url[1024];
    snprintf(url, sizeof(url),
             "https://xmldata.qrz.com/xml/current/?username=%s;password=%s;agent=ham-tools-c-1.0",
             eu, ep);
    free(eu); free(ep);

    ham_buf body = {0};
    if (ham_http_get(url, "ham-tools-c", &body) != 0) return -1;

    xmlDoc *doc = xmlReadMemory(body.data, (int)body.size, "resp.xml", NULL, 0);
    ham_buf_free(&body);
    if (!doc) return -1;

    xmlNode *root = xmlDocGetRootElement(doc);
    xmlNode *sess = root ? find_child(root, "Session") : NULL;
    char *key = sess ? find_child_text(sess, "Key") : NULL;
    char *err = sess ? find_child_text(sess, "Error") : NULL;

    xmlFreeDoc(doc);

    if (err) {
        fprintf(stderr, "qrz: login error: %s\n", err);
        free(err);
    }
    if (!key) return -1;

    free(a->session_key);
    a->session_key = key;
    return 0;
}

int ham_qrzapi_lookup(ham_qrzapi *a, const char *call, ham_fields *out) {
    if (!a->session_key) {
        if (do_login(a) != 0) return -1;
    }

    char *ek = ham_url_encode(a->session_key);
    char *ec = ham_url_encode(call);
    if (!ek || !ec) { free(ek); free(ec); return -1; }

    char url[1024];
    snprintf(url, sizeof(url),
             "https://xmldata.qrz.com/xml/current/?s=%s;callsign=%s", ek, ec);
    free(ek); free(ec);

    ham_buf body = {0};
    if (ham_http_get(url, "ham-tools-c", &body) != 0) return -1;

    xmlDoc *doc = xmlReadMemory(body.data, (int)body.size, "resp.xml", NULL, 0);
    ham_buf_free(&body);
    if (!doc) return -1;

    xmlNode *root = xmlDocGetRootElement(doc);
    xmlNode *sess = root ? find_child(root, "Session") : NULL;
    char *err = sess ? find_child_text(sess, "Error") : NULL;

    /* Session timeout -> re-login once */
    if (err && (strcasestr(err, "Session Timeout") || strcasestr(err, "Invalid session key"))) {
        free(err);
        xmlFreeDoc(doc);
        free(a->session_key);
        a->session_key = NULL;
        if (do_login(a) != 0) return -1;
        return ham_qrzapi_lookup(a, call, out);
    }

    xmlNode *c = root ? find_child(root, "Callsign") : NULL;
    if (!c) {
        if (err) { fprintf(stderr, "qrz: %s\n", err); free(err); }
        xmlFreeDoc(doc);
        return -1;
    }
    free(err);

    for (xmlNode *n = c->children; n; n = n->next) {
        if (n->type != XML_ELEMENT_NODE) continue;
        xmlChar *t = xmlNodeGetContent(n);
        if (t && *t) ham_fields_set(out, (const char *)n->name, (const char *)t);
        if (t) xmlFree(t);
    }

    xmlFreeDoc(doc);
    return 0;
}

/* Best-effort home call extraction. */
char *ham_qrzapi_homecall(const char *call) {
    if (!call) return NULL;
    char buf[64];
    size_t n = 0;
    for (const char *p = call; *p && n + 1 < sizeof(buf); p++) buf[n++] = (char)toupper((unsigned char)*p);
    buf[n] = '\0';

    /* Strip common trailing suffix after '/' */
    static const char *suffixes[] = {
        "/P","/M","/MM","/AM","/QRP","/A","/B","/T", NULL
    };
    for (int i = 0; suffixes[i]; i++) {
        size_t sl = strlen(suffixes[i]);
        if (n >= sl && strcmp(buf + n - sl, suffixes[i]) == 0) {
            buf[n - sl] = '\0';
            n -= sl;
            break;
        }
    }

    /* If still contains '/', pick the segment most-likely a callsign (has a digit). */
    char *slash = strchr(buf, '/');
    if (slash) {
        *slash = '\0';
        char *a = buf, *b = slash + 1;
        int a_has_digit = 0, b_has_digit = 0;
        for (char *p = a; *p; p++) if (isdigit((unsigned char)*p)) { a_has_digit = 1; break; }
        for (char *p = b; *p; p++) if (isdigit((unsigned char)*p)) { b_has_digit = 1; break; }
        if (b_has_digit && !a_has_digit) return strdup(b);
        if (a_has_digit && !b_has_digit) return strdup(a);
        /* Both or neither -> pick the longer */
        return strdup(strlen(b) > strlen(a) ? b : a);
    }

    return strdup(buf);
}
