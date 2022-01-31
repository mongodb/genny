#ifndef URL_HPP
#define URL_HPP

#include <boost/regex.hpp>
#include <boost/utility/string_view.hpp>

#include <algorithm>
#include <iterator>
#include <string>

namespace simple_http {

/**
 * @brief The url class a representation of an HTTP url with defaults for port and scheme
 */
class url
{
    static auto view_start(const std::string& representation, const boost::string_view& view)
    {
        return view.empty() ? 0 : std::distance<const char*>(representation.data(), begin(view));
    }

public:
    static constexpr boost::string_view SchemeHttp() { return "http"; }
    static constexpr boost::string_view SchemeHttps() { return "https"; }
    static constexpr boost::string_view SchemeFtp() { return "ftp"; }

    url() noexcept = default;

    url(boost::string_view url) : m_representation{url.to_string()} { parseRepresentation(); }

    url(boost::string_view host, boost::string_view target,
        boost::string_view scheme = boost::string_view{},
        boost::string_view port = boost::string_view{},
        boost::string_view username = boost::string_view{},
        boost::string_view password = boost::string_view{})
    {
        size_t host_off{};
        size_t port_off{};
        size_t username_off{};
        size_t password_off{};
        size_t target_off{};
        m_representation.reserve(host.length() + scheme.length() + port.length() +
                                 username.length() + password.length() + 6);
        if (!scheme.empty()) {
            m_representation = scheme.to_string();
            m_representation += "://";
        }
        if (!username.empty()) {
            username_off = m_representation.length();
            m_representation += username.to_string();
            if (!password.empty()) {
                m_representation += ':';
                password_off = m_representation.length();
                m_representation += password.to_string();
                m_representation += '@';
            }
        }
        host_off = m_representation.length();
        m_representation += host.to_string();
        if (!port.empty()) {
            m_representation += ':';
            port_off = m_representation.length();
            m_representation += port.to_string();
        }
        target_off = m_representation.length();
        m_representation += target.to_string();
        m_host = boost::string_view{m_representation.data() + host_off, host.length()};
        m_target = boost::string_view{m_representation.data() + target_off, target.length()};
        if (!scheme.empty()) {
            m_scheme = boost::string_view{m_representation.data(), scheme.length()};
        }
        if (!username.empty()) {
            m_username =
                boost::string_view{m_representation.data() + username_off, username.length()};
            if (password_off > 0) {
                m_password =
                    boost::string_view{m_representation.data() + password_off, password.length()};
            }
        }
        if (!port.empty()) {
            m_port = boost::string_view{m_representation.data() + port_off, port.length()};
        }
        const auto sep = m_target.find('?');
        if (sep != m_target.npos) {
            size_t query_off = target_off + sep + 1;
            m_path = boost::string_view{m_representation.data() + target_off, sep};
            m_query =
                boost::string_view{m_representation.data() + query_off, target.length() - sep - 1};
        } else {
            m_path = m_target;
        }
    }

    url(const url& other) noexcept : m_representation{other.m_representation}
    {
        viewsFromOther(other);
    }

    url(url&& other) noexcept { *this = std::move(other); }

    url& operator=(boost::string_view url)
    {
        m_representation = url.to_string();
        parseRepresentation();
        return *this;
    }

    url& operator=(const url& other)
    {
        m_representation = other.m_representation;
        viewsFromOther(other);
        return *this;
    }

    url& operator=(url&& other) noexcept
    {
        m_representation = other.m_representation;
        viewsFromOther(other);
        return *this;
    }

    bool valid() const noexcept
    {
        // Minimum is just a hostname
        return !m_host.empty();
    }

    boost::string_view scheme() const
    {
        if (m_scheme.empty() && valid()) {
            return SchemeHttp();
        }
        return m_scheme;
    }

    boost::string_view host() const { return m_host; }

    boost::string_view port() const
    {
        if (m_port.empty() && valid()) {
            return m_scheme == SchemeHttps()
                ? DefaultHttpsPort()
                : m_scheme == SchemeFtp() ? DefaultFtpPort() : DefaultHttpPort();
        }
        return m_port;
    }

    boost::string_view username() const { return m_username; }

    boost::string_view password() const { return m_password; }

    boost::string_view target() const
    {
        if (m_target.empty() && valid()) {
            return DefaultTarget();
        }
        return m_target;
    }

    boost::string_view path() const { return m_path; }

    boost::string_view query() const { return m_query; }

    bool hasAuthentication() const { return !m_username.empty() && !m_password.empty(); }

    void setUsername(boost::string_view username) { m_username = username; }

    void setPassword(boost::string_view password) { m_password = password; }

    void setScheme(boost::string_view scheme) { m_scheme = scheme; }

private:
    static constexpr boost::string_view DefaultTarget() { return "/"; }
    static constexpr boost::string_view DefaultScheme() { return SchemeHttp(); };
    static constexpr boost::string_view DefaultHttpPort() { return "80"; }
    static constexpr boost::string_view DefaultHttpsPort() { return "443"; }
    static constexpr boost::string_view DefaultFtpPort() { return "21"; }

    void parseRepresentation()
    {
        constexpr size_t SchemeLoc{2};
        constexpr size_t UserLoc{4};
        constexpr size_t PassLoc{6};
        constexpr size_t HostLoc{7};
        constexpr size_t PortLoc{9};
        constexpr size_t TargetLoc{10};
        constexpr size_t PathLoc{11};
        constexpr size_t QueryLoc{14};

        static const boost::regex http_reg(
            "^((https?|ftp):\\/\\/)?"                                   // scheme
            "(([^\\s$.?#].?[^\\s\\/:]*)(:([^\\s$.?#].?[^\\s\\/]*))?@)?" // auth
            "([^\\s$.?#].[^\\s\\/:]+)"                                  // host
            "(:([0-9]+))?"                                              // port
            "(([^\\s?#]*)?(([\\?#])([^\\s]*))?)?$");                    // target (path?query)
        boost::string_view represent{m_representation};
        auto start = represent.cbegin();
        auto end = represent.cend();
        boost::match_results<boost::string_view::const_iterator> matches;
        boost::match_flag_type flags = boost::match_default;
        if (boost::regex_match(start, end, matches, http_reg, flags)) {
            boost::string_view::size_type size = static_cast<boost::string_view::size_type>(
                std::distance(matches[SchemeLoc].first, matches[SchemeLoc].second));
            m_scheme = boost::string_view{matches[SchemeLoc].first, size};
            size = static_cast<boost::string_view::size_type>(
                std::distance(matches[UserLoc].first, matches[UserLoc].second));
            m_username = boost::string_view{matches[UserLoc].first, size};
            size = static_cast<boost::string_view::size_type>(
                std::distance(matches[PassLoc].first, matches[PassLoc].second));
            m_password = boost::string_view{matches[PassLoc].first, size};
            size = static_cast<boost::string_view::size_type>(
                std::distance(matches[HostLoc].first, matches[HostLoc].second));
            m_host = boost::string_view{matches[HostLoc].first, size};
            size = static_cast<boost::string_view::size_type>(
                std::distance(matches[PortLoc].first, matches[PortLoc].second));
            m_port = boost::string_view{matches[PortLoc].first, size};
            size = static_cast<boost::string_view::size_type>(
                std::distance(matches[TargetLoc].first, matches[TargetLoc].second));
            m_target = boost::string_view{matches[TargetLoc].first, size};
            size = static_cast<boost::string_view::size_type>(
                std::distance(matches[PathLoc].first, matches[PathLoc].second));
            m_path = boost::string_view{matches[PathLoc].first, size};
            size = static_cast<boost::string_view::size_type>(
                std::distance(matches[QueryLoc].first, matches[QueryLoc].second));
            m_query = boost::string_view{matches[QueryLoc].first, size};
        }
    }

    void viewsFromOther(const url& other)
    {
        if (!other.m_scheme.empty()) {
            m_scheme = {m_representation.data(), other.m_scheme.size()};
        } else {
            m_scheme.clear();
        }
        if (!other.m_host.empty()) {
            const auto hostStart = view_start(other.m_representation, other.m_host);
            m_host = {m_representation.data() + hostStart, other.m_host.size()};
        } else {
            m_host.clear();
        }
        if (!other.m_port.empty()) {
            const auto portStart = view_start(other.m_representation, other.m_port);
            m_port = {m_representation.data() + portStart, other.m_port.size()};
        } else {
            m_port.clear();
        }
        if (!other.m_username.empty()) {
            const auto usernameStart = view_start(other.m_representation, other.m_username);
            m_username = {m_representation.data() + usernameStart, other.m_username.size()};
        } else {
            m_username.clear();
        }
        if (!other.m_password.empty()) {
            const auto passwordStart = view_start(other.m_representation, other.m_password);
            m_password = {m_representation.data() + passwordStart, other.m_password.size()};
        } else {
            m_password.clear();
        }
        if (!other.m_target.empty()) {
            const auto targetStart = view_start(other.m_representation, other.m_target);
            m_target = {m_representation.data() + targetStart, other.m_target.size()};
        } else {
            m_path.clear();
        }
        if (!other.m_path.empty()) {
            const auto pathStart = view_start(other.m_representation, other.m_path);
            m_path = {m_representation.data() + pathStart, other.m_path.size()};
        } else {
            m_path.clear();
        }
        if (!other.m_query.empty()) {
            const auto queryStart = view_start(other.m_representation, other.m_query);
            m_query = {m_representation.data() + queryStart, other.m_query.size()};
        } else {
            m_query.clear();
        }
    }

    std::string m_representation;
    boost::string_view m_scheme;
    boost::string_view m_host;
    boost::string_view m_port;
    boost::string_view m_username;
    boost::string_view m_password;
    boost::string_view m_path;
    boost::string_view m_target;
    boost::string_view m_query;
};

} // namespace simple_http

#endif // URL_HPP
