#ifndef OSMIUM_GEOM_PROJECTION_HPP
#define OSMIUM_GEOM_PROJECTION_HPP

/*

This file is part of Osmium (https://osmcode.org/libosmium).

Copyright 2013-2019 Jochen Topf <jochen@topf.org> and others (see README).

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

/**
 * @file
 *
 * This file contains code for projecting OSM locations to arbitrary
 * coordinate reference systems. It is based on the Proj.4 library.
 *
 * @attention If you include this file, you'll need to link with `libproj`.
 */

#include <osmium/geom/coordinates.hpp>
#include <osmium/geom/mercator_projection.hpp>
#include <osmium/geom/util.hpp>
#include <osmium/osm/location.hpp>

#ifdef __has_include
#if __has_include(<proj.h>)
#include <proj.h>
#define PROJ_V4 0
#elif __has_include(<proj_api.h>)
#include <proj_api.h>
#define PROJ_V4 1
#else
#error 'No proj-lib found'
#endif
#else
#include <proj_api.h>
#define PROJ_V4 1
#endif

#include <memory>
#include <string>

namespace osmium {

    namespace geom {

        /**
         * C++ wrapper for a Coordinate Reference System of the proj library.
         */
#if PROJ_V4 == 1

        class CRS {

            struct ProjCRSDeleter {

                void operator()(void* crs) {
                    pj_free(crs);
                }
            }; // struct ProjCRSDeleter

            std::unique_ptr<void, ProjCRSDeleter> m_crs;

        public:

            explicit CRS(const char* crs) :
            m_crs(pj_init_plus(crs), ProjCRSDeleter()) {
                if (!m_crs) {
                    throw osmium::projection_error{std::string{"creation of CRS failed: "} +pj_strerrno(*pj_get_errno_ref())};
                }
            }

            explicit CRS(const std::string& crs) :
            CRS(crs.c_str()) {
            }

            explicit CRS(int epsg) :
            CRS(std::string{"+init=epsg:"}
            +std::to_string(epsg)) {
            }

            /**
             * Get underlying projPJ handle from proj library.
             */
            projPJ get() const noexcept {
                return m_crs.get();
            }

            bool is_latlong() const noexcept {
                return pj_is_latlong(m_crs.get()) != 0;
            }

            bool is_geocent() const noexcept {
                return pj_is_geocent(m_crs.get()) != 0;
            }

        }; // class CRS

        /**
         * Transform coordinates from one CRS into another. Wraps the same
         * function of the proj library.
         *
         * Coordinates have to be in radians and are produced in radians.
         *
         * @throws osmium::projection_error if the projection fails
         */
        // cppcheck-suppress passedByValue (because c is small and we want to change it)

        inline Coordinates transform(const CRS& src, const CRS& dest, Coordinates c) {
            const int result = pj_transform(src.get(), dest.get(), 1, 1, &c.x, &c.y, nullptr);
            if (result != 0) {
                throw osmium::projection_error{std::string{"projection failed: "} +pj_strerrno(result)};
            }
            return c;
        }
#else

        class CRS {

            struct ProjCRSDeleter {

                void operator()(PJconsts* crs) {
                    proj_destroy(crs);
                }
            }; // struct ProjCRSDeleter
            std::unique_ptr<PJconsts, ProjCRSDeleter> m_crs;
            std::string m_crs_string;

        public:

            explicit CRS(const std::string& crs) :
            m_crs(proj_create(PJ_DEFAULT_CTX, crs.c_str()), ProjCRSDeleter()) {
                if (!m_crs) {
                    throw osmium::projection_error{std::string{"creation of CRS failed: "} +proj_errno_string(proj_errno(proj_create(PJ_DEFAULT_CTX, crs.c_str())))};
                }
                this->m_crs_string = crs;
            }

            explicit CRS(const char* crs) :
            CRS(std::string(crs)) {
            }

            explicit CRS(int epsg) :
            CRS(std::string{"+init=epsg:"}
            +std::to_string(epsg)) {
            }

            /**
             * Get underlying projPJ handle from proj library.
             */
            PJconsts* get() const noexcept {
                return const_cast<PJconsts *> (m_crs.get());
            }

            /**
             * Get underlying projPJ handle from proj library.
             */
            const std::string get_crs() const noexcept {
                return m_crs_string;
            }

            bool is_latlong() const noexcept {
                return proj_get_type(const_cast<PJconsts *> (m_crs.get())) == PJ_TYPE_GEOGRAPHIC_2D_CRS;
            }

            bool is_geocent() const noexcept {
                return proj_get_type(const_cast<PJconsts *> (m_crs.get())) == PJ_TYPE_GEOCENTRIC_CRS;
            }

        }; // class CRS

        /**
         * Transform coordinates from one CRS into another. Wraps the same
         * function of the proj library.
         *
         * Coordinates have to be in radians and are produced in radians.
         *
         * @throws osmium::projection_error if the projection fails
         */
        // cppcheck-suppress passedByValue (because c is small and we want to change it)

        inline Coordinates transform(const CRS& src, const CRS& dest, Coordinates c) {
            PJ *P;
            PJ_COORD a, result;

            P = proj_create_crs_to_crs(PJ_DEFAULT_CTX, src.get_crs().c_str(), dest.get_crs().c_str(), NULL);
            a = proj_coord(c.x, c.y, 0, 0);
            result = proj_trans(P, PJ_FWD, a);
            if (proj_errno(P) != 0) {
                throw osmium::projection_error{std::string{"projection failed: "} +proj_errno_string(proj_errno(P))};
            }

            c.x = result.xy.x;
            c.y = result.xy.y;
            return c;
        }
#endif

        /**
         * Functor that does projection from WGS84 (EPSG:4326) to the given
         * CRS.
         *
         * If this Projection is initialized with the constructor taking
         * an integer with the epsg code 4326, no projection is done. If it
         * is initialized with epsg code 3857 the Osmium-internal
         * implementation of the Mercator projection is used, otherwise this
         * falls back to using the proj.4 library. Note that this "magic" does
         * not work if you use any of the constructors taking a string.
         */
        class Projection {
            int m_epsg;
            std::string m_proj_string;
            CRS m_crs_wgs84{4326};
            CRS m_crs_user;

        public:

            explicit Projection(const std::string& proj_string) :
            m_epsg(-1),
            m_proj_string(proj_string),
            m_crs_user(proj_string) {
            }

            explicit Projection(const char* proj_string) :
            m_epsg(-1),
            m_proj_string(proj_string),
            m_crs_user(proj_string) {
            }

            explicit Projection(int epsg) :
            m_epsg(epsg),
            m_proj_string(std::string{"+init=epsg:"}
            +std::to_string(epsg)),
            m_crs_user(epsg) {
            }

            /**
             * Do coordinate transformation.
             *
             * @pre Location must be in valid range (depends on projection
             *      used).
             */
            Coordinates operator()(osmium::Location location) const {
                if (m_epsg == 4326) {
                    return Coordinates{location.lon(), location.lat()};
                }

                if (m_epsg == 3857) {
                    return Coordinates{detail::lon_to_x(location.lon()),
                        detail::lat_to_y(location.lat())};
                }

                Coordinates c{transform(m_crs_wgs84, m_crs_user, Coordinates
                    {deg_to_rad(location.lon()),
                        deg_to_rad(location.lat())})};
                if (m_crs_user.is_latlong()) {
                    c.x = rad_to_deg(c.x);
                    c.y = rad_to_deg(c.y);
                }

                return c;
            }

            int epsg() const noexcept {
                return m_epsg;
            }

            std::string proj_string() const {
                return m_proj_string;
            }

        }; // class Projection

    } // namespace geom

} // namespace osmium

#endif // OSMIUM_GEOM_PROJECTION_HPP
