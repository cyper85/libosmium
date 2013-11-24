#ifndef OSMIUM_HANDLER_HPP
#define OSMIUM_HANDLER_HPP

/*

This file is part of Osmium (http://osmcode.org/osmium).

Copyright 2013 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <stdexcept>

#include <osmium/osm.hpp>
#include <osmium/memory/item.hpp>
#include <osmium/io/input_iterator.hpp>

namespace osmium {

    namespace handler {

        class Handler {

        public:

            void node(const osmium::Node&) const {
            }

            void way(const osmium::Way&) const {
            }

            void relation(const osmium::Relation&) const {
            }

            void changeset(const osmium::Changeset&) const {
            }

            void init() const {
            }

            void before_nodes() const {
            }

            void after_nodes() const {
            }

            void before_ways() const {
            }

            void after_ways() const {
            }

            void before_relations() const {
            }

            void after_relations() const {
            }

            void before_changesets() const {
            }

            void after_changesets() const {
            }

            void done() const {
            }

        }; // class Handler

        namespace detail {

            template <class THandler>
            inline void apply_before_and_after_recurse(osmium::item_type last, osmium::item_type current, THandler& handler) {
                switch (last) {
                    case osmium::item_type::undefined:
                        handler.init();
                        break;
                    case osmium::item_type::node:
                        handler.after_nodes();
                        break;
                    case osmium::item_type::way:
                        handler.after_ways();
                        break;
                    case osmium::item_type::relation:
                        handler.after_relations();
                        break;
                    case osmium::item_type::changeset:
                        handler.after_changesets();
                        break;
                    default:
                        break;
                }
                switch (current) {
                    case osmium::item_type::undefined:
                        handler.done();
                        break;
                    case osmium::item_type::node:
                        handler.before_nodes();
                        break;
                    case osmium::item_type::way:
                        handler.before_ways();
                        break;
                    case osmium::item_type::relation:
                        handler.before_relations();
                        break;
                    case osmium::item_type::changeset:
                        handler.before_changesets();
                        break;
                    default:
                        break;
                }
            }

            template <class THandler, class ...TRest>
            inline void apply_before_and_after_recurse(osmium::item_type last, osmium::item_type current, THandler& handler, TRest&... more) {
                apply_before_and_after_recurse(last, current, handler);
                apply_before_and_after_recurse(last, current, more...);
            }

            template <typename T, typename U>
            using MaybeConst = typename std::conditional<std::is_const<T>::value, typename std::add_const<U>::type, U>::type;

            template <class THandler, class TItem>
            static void switch_on_type(THandler& handler, TItem& item) {
                switch (item.type()) {
                    case osmium::item_type::node:
                        handler.node(static_cast<MaybeConst<TItem, osmium::Node>&>(item));
                        break;
                    case osmium::item_type::way:
                        handler.way(static_cast<MaybeConst<TItem, osmium::Way>&>(item));
                        break;
                    case osmium::item_type::relation:
                        handler.relation(static_cast<MaybeConst<TItem, osmium::Relation>&>(item));
                        break;
                    case osmium::item_type::changeset:
                        handler.changeset(static_cast<MaybeConst<TItem, osmium::Changeset>&>(item));
                        break;
                    default:
                        throw std::runtime_error("unknown type");
                }
            }

            template <class THandler>
            static void switch_on_type(THandler& handler, osmium::Object& item) {
                switch (item.type()) {
                    case osmium::item_type::node:
                        handler.node(static_cast<osmium::Node&>(item));
                        break;
                    case osmium::item_type::way:
                        handler.way(static_cast<osmium::Way&>(item));
                        break;
                    case osmium::item_type::relation:
                        handler.relation(static_cast<osmium::Relation&>(item));
                        break;
                    default:
                        throw std::runtime_error("unknown type");
                }
            }

            template <class THandler>
            static void switch_on_type(THandler& handler, const osmium::Object& item) {
                switch (item.type()) {
                    case osmium::item_type::node:
                        handler.node(static_cast<const osmium::Node&>(item));
                        break;
                    case osmium::item_type::way:
                        handler.way(static_cast<const osmium::Way&>(item));
                        break;
                    case osmium::item_type::relation:
                        handler.relation(static_cast<const osmium::Relation&>(item));
                        break;
                    default:
                        throw std::runtime_error("unknown type");
                }
            }

            template <class THandler, class TItem>
            inline void apply_item_recurse(TItem& item, THandler& handler) {
                switch_on_type(handler, item);
            }

            template <class THandler, class TItem, class ...TRest>
            inline void apply_item_recurse(TItem& item, THandler& handler, TRest&... more) {
                apply_item_recurse(item, handler);
                apply_item_recurse(item, more...);
            }

        } // namespace detail

        template <class TIterator, class ...THandlers>
        inline void apply(TIterator it, TIterator end, THandlers&... handlers) {
            osmium::item_type last_type = osmium::item_type::undefined;
            for (; it != end; ++it) {
                if (last_type != it->type()) {
                    osmium::handler::detail::apply_before_and_after_recurse(last_type, it->type(), handlers...);
                    last_type = it->type();
                }
                osmium::handler::detail::apply_item_recurse(*it, handlers...);
            }
            osmium::handler::detail::apply_before_and_after_recurse(last_type, osmium::item_type::undefined, handlers...);
        }

        template <class TSource, class ...THandlers>
        inline void apply(TSource& source, THandlers&... handlers) {
            apply(osmium::io::InputIterator<TSource>{source},
                  osmium::io::InputIterator<TSource>{},
                  handlers...);
        }

        template <class ...THandlers>
        inline void apply(osmium::memory::Buffer& buffer, THandlers&... handlers) {
            apply(buffer.begin(), buffer.end(), handlers...);
        }

    } // namspace handler

} // namespace osmium

#endif // OSMIUM_HANDLER_HPP
