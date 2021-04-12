/******************************************************************************
 * Copyright © 2013-2021 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Komodo Platform software, including this file may be copied, modified,     *
 * propagated or distributed except according to the terms contained in the   *
 * LICENSE file                                                               *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

//! Project headers
#include "qt.orderbook.hpp"
#include "atomicdex/pages/qt.trading.page.hpp"
#include "atomicdex/services/mm2/mm2.service.hpp"
#include "atomicdex/services/price/orderbook.scanner.service.hpp"

namespace atomic_dex
{
    qt_orderbook_wrapper::qt_orderbook_wrapper(ag::ecs::system_manager& system_manager, entt::dispatcher& dispatcher, QObject* parent) :
        QObject(parent), m_system_manager(system_manager), m_dispatcher(dispatcher),
        m_asks(new orderbook_model(orderbook_model::kind::asks, system_manager, this)),
        m_bids(new orderbook_model(orderbook_model::kind::bids, system_manager, this)),
        m_best_orders(new orderbook_model(orderbook_model::kind::best_orders, system_manager, this))
    {
    }

    bool
    atomic_dex::qt_orderbook_wrapper::is_best_orders_busy() const
    {
        return this->m_system_manager.get_system<orderbook_scanner_service>().is_best_orders_busy();
    }

    atomic_dex::orderbook_model*
    atomic_dex::qt_orderbook_wrapper::get_asks() const
    {
        return m_asks;
    }

    orderbook_model*
    qt_orderbook_wrapper::get_bids() const
    {
        return m_bids;
    }

    atomic_dex::orderbook_model*
    atomic_dex::qt_orderbook_wrapper::get_best_orders() const
    {
        return m_best_orders;
    }

    void
    qt_orderbook_wrapper::refresh_orderbook(t_orderbook_answer answer)
    {
        this->m_asks->refresh_orderbook(answer.asks);
        this->m_bids->refresh_orderbook(answer.bids);
        const auto data = this->m_system_manager.get_system<orderbook_scanner_service>().get_data();
        if (data.empty())
        {
            m_best_orders->clear_orderbook();
        }
        else if (m_best_orders->rowCount() == 0)
        {
            m_best_orders->reset_orderbook(data);
        }
        else
        {
            m_best_orders->refresh_orderbook(data);
        }
        this->set_both_taker_vol();
    }

    void
    qt_orderbook_wrapper::reset_orderbook(t_orderbook_answer answer)
    {
        this->m_asks->reset_orderbook(answer.asks);
        this->m_bids->reset_orderbook(answer.bids);
        this->set_both_taker_vol();
        if (m_selected_best_order->has_value())
        {
            SPDLOG_INFO("selected best orders have a value - set preffered order");
            m_system_manager.get_system<trading_page>().set_preffered_order(m_selected_best_order->value());
            m_selected_best_order = std::nullopt;
        }
        m_best_orders->clear_orderbook();                                                     ///< Remove all elements from the model
        this->m_system_manager.get_system<orderbook_scanner_service>().process_best_orders(); ///< re process the model
    }

    void
    qt_orderbook_wrapper::clear_orderbook()
    {
        this->m_asks->clear_orderbook();
        this->m_bids->clear_orderbook();
        this->m_best_orders->clear_orderbook();
    }

    QVariant
    qt_orderbook_wrapper::get_base_max_taker_vol() const
    {
        return m_base_max_taker_vol;
    }

    QVariant
    qt_orderbook_wrapper::get_rel_max_taker_vol() const
    {
        return m_rel_max_taker_vol;
    }

    void
    atomic_dex::qt_orderbook_wrapper::set_both_taker_vol()
    {
        auto&& [base, rel]         = m_system_manager.get_system<mm2_service>().get_taker_vol();
        this->m_base_max_taker_vol = QJsonObject{
            {"denom", QString::fromStdString(base.denom)}, {"numer", QString::fromStdString(base.numer)}, {"decimal", QString::fromStdString(base.decimal)}};
        emit baseMaxTakerVolChanged();
        this->m_rel_max_taker_vol = QJsonObject{
            {"denom", QString::fromStdString(rel.denom)}, {"numer", QString::fromStdString(rel.numer)}, {"decimal", QString::fromStdString(rel.decimal)}};
        emit relMaxTakerVolChanged();

        auto&& [min_base, min_rel] = m_system_manager.get_system<mm2_service>().get_min_vol();
        this->m_base_min_taker_vol = QString::fromStdString(min_base.min_trading_vol);
        emit baseMinTakerVolChanged();
        this->m_rel_min_taker_vol = QString::fromStdString(min_rel.min_trading_vol);
        emit relMinTakerVolChanged();

        emit currentMinTakerVolChanged();
    }
} // namespace atomic_dex

//! Q_INVOKABLE
namespace atomic_dex
{
    void
    qt_orderbook_wrapper::refresh_best_orders()
    {
        if (safe_float(m_system_manager.get_system<trading_page>().get_volume().toStdString()) > 0)
        {
            this->m_system_manager.get_system<orderbook_scanner_service>().process_best_orders();
        }
        else
        {
            get_best_orders()->clear_orderbook();
        }
    }

    void
    qt_orderbook_wrapper::select_best_order(const QString& order_uuid)
    {
        QVariantMap out;
        const bool  is_buy = m_system_manager.get_system<trading_page>().get_market_mode() == MarketMode::Buy;
        const auto  res    = m_best_orders->match(m_best_orders->index(0, 0), orderbook_model::UUIDRole, order_uuid, 1, Qt::MatchFlag::MatchExactly);
        if (!res.empty())
        {
            const QModelIndex& idx   = res.at(0);
            t_order_contents   order = m_best_orders->get_order_content(idx);
            out["coin"]              = QString::fromStdString(is_buy ? order.rel_coin.value() : order.coin);
            out["price"]             = QString::fromStdString(order.price);
            out["quantity"]          = QString::fromStdString(order.maxvolume);
            out["price_denom"]       = QString::fromStdString(order.price_fraction_denom);
            out["price_numer"]       = QString::fromStdString(order.price_fraction_numer);
            out["quantity_denom"]    = QString::fromStdString(order.max_volume_fraction_denom);
            out["quantity_numer"]    = QString::fromStdString(order.max_volume_fraction_numer);
            m_selected_best_order    = out;
            auto& trading_pg         = m_system_manager.get_system<trading_page>();
            if (!trading_pg.set_pair(false, QString::fromStdString(is_buy ? order.rel_coin.value() : order.coin)))
            {
                //! If we are not able to set the selected pair reset immediatly
                SPDLOG_ERROR("Was not able to set rel coin in the orderbook to : {}", is_buy ? order.rel_coin.value() : order.coin);
                m_selected_best_order = std::nullopt;
            }
        }
    }
    QString
    qt_orderbook_wrapper::get_base_min_taker_vol() const
    {
        return m_base_min_taker_vol.isEmpty() ? "0" : m_base_min_taker_vol;
    }

    QString
    qt_orderbook_wrapper::get_rel_min_taker_vol() const
    {
        return m_rel_min_taker_vol.isEmpty() ? "0" : m_rel_min_taker_vol;
    }

    QString
    qt_orderbook_wrapper::get_current_min_taker_vol() const
    {
        bool is_buy = m_system_manager.get_system<trading_page>().get_market_mode() == MarketMode::Buy;
        t_float_50 volume = is_buy ? safe_float(get_rel_min_taker_vol().toStdString()) : safe_float(get_base_min_taker_vol().toStdString());
        t_float_50 price = safe_float(m_system_manager.get_system<trading_page>().get_price().toStdString());
        t_float_50 threshold = volume * price;
        return QString::fromStdString(utils::format_float(threshold));
    }
} // namespace atomic_dex
