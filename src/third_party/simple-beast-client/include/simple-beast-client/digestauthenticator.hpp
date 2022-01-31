#ifndef DIGESTAUTHENTICATOR_H
#define DIGESTAUTHENTICATOR_H

#include <boost/algorithm/string.hpp>
#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/regex.hpp>
#include <boost/utility/string_view.hpp>
#include <cppcodec/hex_default_lower.hpp>
#include <cstdint>
#include <openssl/md5.h>
#include <random>
#include <string>

namespace simple_http {

class digest_authenticator
{
public:
    digest_authenticator(boost::string_view www_authenticate, boost::string_view username,
                         boost::string_view password, boost::string_view uri,
                         boost::string_view method, boost::string_view responseBody) noexcept
      : m_authenticate{www_authenticate}
      , m_username{username}
      , m_password{password}
      , m_uri{uri}
      , m_method{method}
      , m_body{responseBody}
      , m_qop{None}
    {}

    bool generateAuthorization()
    {
        // Nonce and realm are both required for digest athentication.
        if (!findNonce() || !findRealm()) {
            return false;
        }
        findOpaque();
        findQop();
        findAlgorithm();
        m_cnonce = generateNonce();
        m_nonceCount = updateNonceCount();
        MD5_hash ha1;
        calculateHA1(ha1);
        m_ha1 = hex::encode(ha1, sizeof(ha1));
        MD5_hash ha2;
        calculateHA2(ha2);
        m_ha2 = hex::encode(ha2, sizeof(ha2));
        MD5_hash response;
        calculateResponse(response);
        m_response = hex::encode(response, sizeof(response));
        m_authorization = "Digest username=\"";
        m_authorization.reserve(128 + m_username.size() + m_realm.size() + m_nonce.size() +
                                m_uri.size() + m_nonceCount.size() + m_cnonce.size() +
                                m_response.size() + m_opaque.size());
        m_authorization.append(m_username.to_string());
        m_authorization.append("\", realm=\"");
        m_authorization.append(m_realm.to_string());
        m_authorization.append("\", nonce=\"");
        m_authorization.append(m_nonce.to_string());
        m_authorization.append("\", uri=\"");
        m_authorization.append(m_uri.to_string());
        m_authorization.append("\", qop=");
        m_authorization.append(m_qop == AuthInt ? "auth-int" : "auth");
        m_authorization.append(", algorithm=MD5, nc=");
        m_authorization.append(m_nonceCount);
        m_authorization.append(", cnonce=\"");
        m_authorization.append(m_cnonce);
        m_authorization.append("\", response=\"");
        m_authorization.append(m_response);
        if (!m_opaque.empty()) {
            m_authorization.append("\", opaque=\"");
            m_authorization.append(m_opaque.to_string());
        }
        m_authorization.append("\"");
        return true;
    }

    std::string authorization() const noexcept { return m_authorization; }

    static std::string generateNonce()
    {
        std::random_device rd;
        std::uniform_int_distribution<unsigned short> length{8, 32};
        std::uniform_int_distribution<unsigned short> distNum{0, 15};

        std::string nonce;
        nonce.resize(length(rd));
        constexpr char hex[16]{'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
        for (char& val : nonce) {
            val = hex[distNum(rd)];
        }
        return nonce;
    }

private:
    enum QualityOfProtection
    {
        None,
        Auth,
        AuthInt
    };

    using MD5_hash = unsigned char[MD5_DIGEST_LENGTH];

    static std::string updateNonceCount()
    {
        static unsigned int s_nonceCount{};
        return std::to_string(++s_nonceCount);
    }

    bool findNonce() { return findSection("nonce", m_nonce); }

    bool findRealm() { return findSection("realm", m_realm); }

    bool findOpaque() { return findSection("opaque", m_opaque); }

    bool findAlgorithm() { return findSection("algorithm", m_algorithm); }

    bool findQop()
    {
        boost::string_view qop;
        if (findSection("qop", qop)) {
            // auth-int only with response body - working with tested implementations
            if (boost::iequals(qop, "auth-int") && !m_body.empty()) {
                m_qop = AuthInt;
            } else {
                m_qop = Auth;
            }
        }
        return false;
    }

    bool findSection(const std::string& key, boost::string_view& value) const
    {
        boost::regex reg{key + "=([^,]+)"};
        auto start = m_authenticate.cbegin();
        auto end = m_authenticate.cend();
        boost::match_results<boost::string_view::const_iterator> matches;
        boost::match_flag_type flags = boost::match_default;
        if (boost::regex_search(start, end, matches, reg, flags)) {
            size_t size = static_cast<size_t>(std::distance(matches[1].first, matches[1].second));
            start = matches[1].first;
            end = matches[1].second - 1;
            // Trim quotes if they are there.
            if (*start == '"') {
                ++start;
                --size;
            }
            if (*end == '"') {
                --size;
            }
            value = boost::string_view(start, size);
            return true;
        }
        return false;
    }

    void calculateHA1(MD5_hash ha1) noexcept
    {
        MD5_CTX Md5Ctx;
        MD5_Init(&Md5Ctx);
        MD5_Update(&Md5Ctx, m_username.data(), m_username.size());
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, m_realm.data(), m_realm.size());
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, m_password.data(), m_password.size());
        MD5_Final(ha1, &Md5Ctx);
        if (boost::iequals(m_algorithm, "md5-sess")) {
            MD5_Init(&Md5Ctx);
            MD5_Update(&Md5Ctx, ha1, MD5_DIGEST_LENGTH);
            MD5_Update(&Md5Ctx, ":", 1);
            MD5_Update(&Md5Ctx, m_nonce.data(), m_nonce.size());
            MD5_Update(&Md5Ctx, ":", 1);
            MD5_Update(&Md5Ctx, m_cnonce.data(), m_cnonce.size());
            MD5_Final(ha1, &Md5Ctx);
        };
    }

    void calculateHA2(MD5_hash ha2) noexcept
    {
        MD5_CTX Md5Ctx;

        // calculate H(A2)
        MD5_Init(&Md5Ctx);
        MD5_Update(&Md5Ctx, m_method.data(), m_method.size());
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, m_uri.data(), m_uri.size());
        if (m_qop == AuthInt) {
            MD5_Update(&Md5Ctx, ":", 1);
            MD5_Update(&Md5Ctx, m_body.data(), m_body.size());
        };
        MD5_Final(ha2, &Md5Ctx);
    }

    void calculateResponse(MD5_hash result) noexcept
    {
        MD5_CTX Md5Ctx;
        // calculate response
        MD5_Init(&Md5Ctx);
        MD5_Update(&Md5Ctx, m_ha1.data(), m_ha1.size());
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, m_nonce.data(), m_nonce.size());
        MD5_Update(&Md5Ctx, ":", 1);
        if (m_qop != None) {
            MD5_Update(&Md5Ctx, m_nonceCount.data(), m_nonceCount.size());
            MD5_Update(&Md5Ctx, ":", 1);
            MD5_Update(&Md5Ctx, m_cnonce.data(), m_cnonce.size());
            MD5_Update(&Md5Ctx, ":", 1);
            if (m_qop == AuthInt) {
                MD5_Update(&Md5Ctx, "auth-int", 8);
            } else {
                MD5_Update(&Md5Ctx, "auth", 4);
            }
            MD5_Update(&Md5Ctx, ":", 1);
        };
        MD5_Update(&Md5Ctx, m_ha2.data(), m_ha2.size());
        MD5_Final(result, &Md5Ctx);
    }

    boost::string_view m_authenticate;
    boost::string_view m_username;
    boost::string_view m_password;
    boost::string_view m_realm;
    boost::string_view m_nonce;
    boost::string_view m_opaque;
    boost::string_view m_algorithm;
    boost::string_view m_uri;
    boost::string_view m_method;
    boost::string_view m_body;

    QualityOfProtection m_qop;
    std::string m_cnonce;
    std::string m_nonceCount;
    std::string m_ha1;
    std::string m_ha2;
    std::string m_response;
    std::string m_authorization;
};

} // namespace simple_http

#endif // DIGESTAUTHENTICATOR_H
