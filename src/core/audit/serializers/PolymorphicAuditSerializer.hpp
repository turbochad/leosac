/*
    Copyright (C) 2014-2016 Islog

    This file is part of Leosac.

    Leosac is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Leosac is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "core/audit/IAuditEntry.hpp"
#include "tools/Serializer.hpp"
#include "tools/Visitor.hpp"
#include "tools/bs2.hpp"
#include <json.hpp>
#include <string>

namespace Leosac
{
using json = nlohmann::json;

namespace Audit
{
namespace Serializer
{
/**
 * A serializer that handle any type of Audit event and will
 * try to perform deep serialization.
 *
 * This class, through the use of other classes in the Audit::Serializer
 * namespace, is able to serialize core Audit objects.
 *
 * However, the audit system being extensible, we need a mechanism for
 * runtime registered serializer: serializers that can handle specific,
 * module-defined audit object (generally, specific audit
 * entry and their serializer comes from the same module).
 *
 * This class provides a `register_serializer(Callable c)` static function
 * to let modules register their serializers.
 *
 * @note The serializer adapter (ie, the callable passed to `register_adapter()`)
 * must return `optional<json>`. The reason for this is: the adapter may be
 * invoked with audit entry that it is not able to marshal. Returning an
 * empty `optional<json>` will let the system knows that this serializer
 * is inadequate for this audit entry.
 *
 */
struct PolymorphicAuditJSON
    : public Leosac::Serializer<json, Audit::IAuditEntry, PolymorphicAuditJSON>
{
    static json serialize(const Audit::IAuditEntry &in, const SecurityContext &sc);

    /**
     * Returns the "type-name" of the audit entry.
     *
     * Possible return value could be "audit-user-event".
     */
    static std::string type_name(const Audit::IAuditEntry &in);

    /**
     * A typedef to callable that must be provided by a client when invoking
     * register_serializer().
     *
     * The corresponding slot must accept a reference to the audit entry to be
     * serialized, aswell as a reference to the security context.
     * It shall returns an `optional<json>`, empty if the serializer doesn't match
     * the audit entry's type.
     */
    using RuntimeSerializerCallable =
        std::shared_ptr<std::function<boost::optional<json>(
            const Audit::IAuditEntry &, const SecurityContext &)>>;

  private:
    /**
     * A combiner for boost::signals2.
     *
     * This combiner will make sure to ignore the return value of
     * serializers that can't handle a given type of audit entry.
     */
    struct RuntimeSerializerCombiner
    {
        using result_type = boost::optional<json>;
        template <typename InputIterator>
        boost::optional<json> operator()(InputIterator first,
                                         InputIterator last) const
        {
            for (; first != last; ++first)
            {
                boost::optional<json> serialized = *first;
                if (serialized)
                    return serialized;
            }
            // No valid serializers, or simply no serializers.
            return boost::none;
        }
    };

    /**
     * The type of signal object used to represents available serializers.
     */
    using RuntimeSerializerSignal =
        bs2::signal<boost::optional<json>(const Audit::IAuditEntry &,
                                          const SecurityContext &),
                    RuntimeSerializerCombiner>;

    /**
     * A signal object that, when triggered, will invoked runtime-registered
     * serializers, giving them a chance to serializer the audit entry.
     */
    static RuntimeSerializerSignal runtime_serializers_;

  public:
    /**
     * Register a dynamic serializer that can handle some subtype of AuditEntry.
     *
     * The serializer takes the form a shared_ptr to an std::function. The reason
     * for this is so we are able to automatically tracks disconnection.
     * (Disconnection will happen if a module that provide a serializer gets
     * unloaded from memory).
     */
    static bs2::connection register_serializer(RuntimeSerializerCallable callable);

  private:
    /**
     * Non static helper that can visit audit object.
     */
    struct HelperSerialize : public Tools::Visitor<Audit::IUserEvent>,
                             public Tools::Visitor<Audit::IWSAPICall>,
                             public Tools::Visitor<Audit::IScheduleEvent>,
                             public Tools::Visitor<Audit::IGroupEvent>,
                             public Tools::Visitor<Audit::ICredentialEvent>,
                             public Tools::Visitor<Audit::IDoorEvent>,
                             public Tools::Visitor<Audit::IUserGroupMembershipEvent>
    {
        HelperSerialize(const SecurityContext &sc);

        void visit(const Audit::IUserEvent &t) override;
        void visit(const Audit::IWSAPICall &t) override;
        void visit(const Audit::IScheduleEvent &t) override;
        void visit(const Audit::IGroupEvent &t) override;
        void visit(const Audit::ICredentialEvent &t) override;
        void visit(const Audit::IDoorEvent &t) override;
        void visit(const Audit::IUserGroupMembershipEvent &t) override;


            /**
             * Called when no "hardcoded" audit type match, this method
             * will delegate to runtime-registered serializer (if any).
             */
        virtual void cannot_visit(const Tools::IVisitable &visitable) override;

        /**
         * Store the result here because we can't return from
         * the visit() method.
         */
        json result_;

        /**
         * Reference to the security context.
         */
        const SecurityContext &security_context_;
    };
};
}
}
}
