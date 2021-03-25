#include <QJsonObject>

#include "self.update.service.hpp"

namespace atomic_dex
{
    void self_update_service::update() noexcept
    {
        using namespace std::chrono_literals;
        
        const auto now = std::chrono::high_resolution_clock::now();
        const auto s   = std::chrono::duration_cast<std::chrono::seconds>(now - clock);
        if (s >= 1h)
        {
            fetch_last_release_info();
            clock = std::chrono::high_resolution_clock::now();
        }
    }
    
    void self_update_service::fetch_last_release_info()
    {
        auto releases_request =
            github_api::repository_releases_request{.owner = "KomodoPlatform", .repository = "atomicDEX-Desktop"};
        
        github_api::get_repository_releases_async(releases_request)
            .then([this](web::http::http_response resp) {
                auto last_release = github_api::get_last_repository_release_from_http_response(resp);
                last_release_info = last_release;
                emit last_release_infoChanged();
            })
            .then(&handle_exception_pplx_task);
    }
    
    void self_update_service::run_update()
    {
    
    }
    
    QVariant self_update_service::get_last_release_info() const noexcept
    {
        auto last_update_info = last_release_info.get();
        return QJsonObject({{"tag_name", QString::fromStdString(last_update_info.tag_name)},
                            {"url", QString::fromStdString(last_update_info.url)}});
    }
}