/*
 wdb

 Copyright (C) 2012 met.no

 Contact information:
 Norwegian Meteorological Institute
 Box 43 Blindern
 0313 OSLO
 NORWAY
 E-mail: wdb@met.no

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 MA  02110-1301, USA
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//project
#include "GribFile.hpp"
#include "GribField.hpp"
#include "GribLoader.hpp"

// wdb
#include <GridGeometry.h>

// libfelt
#include <felt/FeltFile.h>

// libfimex
#include <fimex/CDM.h>
#include <fimex/CDMReader.h>
#include <fimex/CDMExtractor.h>
#include <fimex/CDMException.h>
#include <fimex/CDMconstants.h>
#include <fimex/CDMDimension.h>
#include <fimex/CDMReaderUtils.h>
#include <fimex/CDMInterpolator.h>
#include <fimex/CDMFileReaderFactory.h>

// libpqxx
#include <pqxx/util>

// boost
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

// std
#include <algorithm>
#include <functional>
#include <cmath>
#include <sstream>

using namespace std;
using namespace boost::posix_time;
using namespace boost::filesystem;

namespace {

    path getConfigFile(const path& fileName)
    {
        static const path sysConfDir = "";//./etc/";//SYSCONFDIR;
        path confPath = sysConfDir/fileName;
        return confPath;
    }

    std::string toString(const boost::posix_time::ptime & time )
    {
        if ( time == boost::posix_time::ptime(neg_infin) )
            return "-infinity"; //"1900-01-01 00:00:00+00";
        else if ( time == boost::posix_time::ptime(pos_infin) )
            return "infinity";//"2100-01-01 00:00:00+00";
        // ...always convert to zulu time
        std::string ret = to_iso_extended_string(time) + "+00";
        return ret;
    }
}

namespace wdb { namespace load { namespace point {

    GribLoader::GribLoader(Loader& controller)
        : FileLoader(controller)
    {
        setup();
    }

    GribLoader::~GribLoader()
    {
        // NOOP
    }

    void GribLoader::setup()
    {
        FileLoader::setup();

        if(options().loading().valueparameter2Config.empty())
            throw std::runtime_error("Can't open valueparameter2.config file [empty string?]");
        if(options().loading().levelparameter2Config.empty())
            throw std::runtime_error("Can't open levelparameter2.config file [empty string?]");
        if(options().loading().leveladditions2Config.empty())
            throw std::runtime_error("Can't open leveladditions2.config file [empty string?]");

        // load GRIB2 metadata
        point2ValueParameter2_.open(getConfigFile(options().loading().valueparameter2Config).file_string());
        point2LevelParameter2_.open(getConfigFile(options().loading().levelparameter2Config).file_string());
        point2LevelAdditions2_.open(getConfigFile(options().loading().leveladditions2Config).file_string());
    }

    void GribLoader::loadInterpolated(const string& fileName)
    {
        GribFile file(fileName);

        if(times_.size() == 0)
            return;

        // Get first field, and check if it exists
        GribFile::Field gribField = file.next();
        if(!gribField) {
            // If the file is empty, we need to throw an error
            std::string errorMessage = "End of file was hit before a product was read into file ";
            errorMessage += file.fileName();
            throw std::runtime_error( errorMessage );
        }

        for( ; gribField; gribField = file.next())
        {
            try{
                std::map<std::string, EntryToLoad>::iterator eIt;

                const GribField& field = *gribField;
                editionNumber_ = editionNumber(field);
                std::string name = valueParameterName(field);
                std::string unit = valueParameterUnit(field);
                std::string provider = dataProviderName(field);
                std::vector<Level> levels;
                levelValues(levels, field);

                if(entries2load().find(name) == entries2load().end()) {
                    EntryToLoad entry;
                    entry.name_ = name;
                    entry.unit_ = unit;
                    entry.provider_ = provider;
                    entries2load().insert(std::make_pair<std::string, EntryToLoad>(entry.name_, entry));
                }

                eIt = entries2load().find(name);
                for(size_t i = 0; i < levels.size(); ++i) {
                    eIt->second.levels_.insert(levels[i].levelFrom_);
                    eIt->second.levelname_ = levels[i].levelParameter_;
                }

            } catch ( wdb::ignore_value &e ) {
                std::cerr << e.what() << " Data field not loaded." << std::endl;
            } catch ( std::out_of_range &e ) {
                std::cerr << "Metadata missing for data value. " << e.what() << " Data field not loaded." << std::endl;
            } catch ( std::exception & e ) {
                std::cerr << e.what() << " Data field not loaded." << std::endl;
            }
        }

        loadEntries();

        loadWindEntries();
    }

    string GribLoader::dataProviderName(const GribField & field) const
    {
        stringstream keyStr;
        keyStr << field.getGeneratingCenter() << ", "
               << field.getGeneratingProcess();
        std::cerr << __FUNCTION__ << " keyStr " << keyStr.str() << std::endl;
        try {
            std::string ret = point2DataProviderName_[ keyStr.str() ];
            return ret;
        }
        catch ( std::out_of_range &e ) {
            std::cerr << "Could not identify the data provider." << std::endl;
            throw;
        }
    }

    string GribLoader::valueParameterName(const GribField & field) const
    {
        stringstream keyStr;
        std::string ret;
        if (editionNumber_ == 1) {
            keyStr << field.getGeneratingCenter() << ", "
                   << field.getCodeTableVersionNumber() << ", "
                   << field.getParameter1() << ", "
                   << field.getTimeRange() << ", "
                   << "0, 0, 0, 0"; // Default values for thresholds
            try {
                ret = point2ValueParameter_[keyStr.str()];
            }
            catch ( std::out_of_range &e ) {
                std::cerr << "Could not identify the value parameter." << std::endl;
                throw;
            }
        }
        else {
            keyStr << field.getParameter2();
            std::cerr << __FUNCTION__ << " keyStr " << keyStr.str() << std::endl;
            try {
                ret = point2ValueParameter2_[keyStr.str()];
            }
            catch ( std::out_of_range &e ) {
                std::cerr << "Could not identify the value parameter." << std::endl;
                throw;
            }
        }
        ret = ret.substr( 0, ret.find(',') );
        boost::trim( ret );
        return ret;
    }

    string GribLoader::valueParameterUnit(const GribField & field) const
    {
        stringstream keyStr;
        std::string ret;
        if (editionNumber_ == 1) {
            keyStr << field.getGeneratingCenter() << ", "
                   << field.getCodeTableVersionNumber() << ", "
                   << field.getParameter1() << ", "
                   << field.getTimeRange() << ", "
                   << "0, 0, 0, 0"; // Default values for thresholds
            try {
                ret = point2ValueParameter_[keyStr.str()];
            }
            catch ( std::out_of_range &e ) {
                std::cerr << "Could not identify the value parameter identified by " << keyStr.str() << std::endl;
                throw;
            }
        }
        else {
            keyStr << field.getParameter2();
            std::cerr << __FUNCTION__ << " keyStr " << keyStr.str() << std::endl;
            try {
                ret = point2ValueParameter2_[keyStr.str()];
            }
            catch ( std::out_of_range &e ) {
                std::cerr << "Could not identify the value parameter identified by " << keyStr.str() << std::endl;
                throw;
            }
        }
        ret = ret.substr( ret.find(',') + 1 );
        boost::trim( ret );
        return ret;
    }

    void GribLoader::levelValues( std::vector<wdb::load::Level> & levels, const GribField & field )
    {
        bool ignored = false;
        stringstream keyStr;
        std::string ret;
        try {
            if (editionNumber_ == 1) {
                keyStr << field.getLevelParameter1();
                std::cerr << __FUNCTION__ << " field.getLevelParameter1() "<<keyStr.str() << std::endl;
                ret = point2LevelParameter_[keyStr.str()];
            }
            else {
                keyStr << field.getLevelParameter2();
                std::cerr << __FUNCTION__ << " keyStr "<<keyStr.str() << std::endl;
                ret = point2LevelParameter2_[keyStr.str()];
            }
            std::string levelParameter = ret.substr( 0, ret.find(',') );
            boost::trim( levelParameter );
            std::string levelUnit = ret.substr( ret.find(',') + 1 );
            boost::trim( levelUnit );
            float coeff = 1.0;
            float term = 0.0;
            wdbConnection().readUnit( levelUnit, &coeff, &term );
            float lev1 = field.getLevelFrom();
            float lev2 = field.getLevelTo();
            if ( ( coeff != 1.0 )&&( term != 0.0) ) {
                lev1 =   ( ( lev1 * coeff ) + term );
                lev2 =   ( ( lev2 * coeff ) + term );
            }
            wdb::load::Level baseLevel( levelParameter, lev1, lev2 );
            levels.push_back( baseLevel );
        }
        catch ( wdb::ignore_value &e )
        {
            std::clog << e.what() << std::endl;
            ignored = true;
        }
        catch ( std::out_of_range &e ) {
            std::cerr << "Could not identify the level parameter identified by " << keyStr.str() << std::endl;
        }
        // Find additional level
        try {
            stringstream keyStr;
            std::string ret;
            if (editionNumber_ == 1) {
                keyStr << field.getGeneratingCenter() << ", "
                       << field.getCodeTableVersionNumber() << ", "
                       << field.getParameter1() << ", "
                       << field.getTimeRange() << ", "
                       << "0, 0, 0, 0, "
                       << field.getLevelParameter1(); // Default values for thresholds
                ret = point2LevelAdditions_[keyStr.str()];
            }
            else {
                keyStr << field.getParameter2();
                std::cerr <<__FUNCTION__<<" keyStr "<< keyStr.str() << std::endl;
                ret = point2LevelAdditions2_[keyStr.str()];
            }
            if ( ret.length() != 0 ) {
                std::string levelParameter = ret.substr( 0, ret.find(',') );
                boost::trim( levelParameter );
                string levFrom = ret.substr( ret.find_first_of(',') + 1, ret.find_last_of(',') - (ret.find_first_of(',') + 1) );
                boost::trim( levFrom );
                string levTo = ret.substr( ret.find_last_of(',') + 1 );
                boost::trim( levTo );
                float levelFrom = boost::lexical_cast<float>( levFrom );
                float levelTo = boost::lexical_cast<float>( levTo );
                wdb::load::Level level( levelParameter, levelFrom, levelTo );
                levels.push_back( level );
            }
        }
        catch ( wdb::ignore_value &e )
        {
            std::clog << e.what() << std::endl;
        }
        catch ( std::out_of_range &e )
        {
            // NOOP
        }
        if ( levels.size() == 0 ) {
            if ( ignored )
                throw wdb::ignore_value( "Level key is ignored" );
            else
                throw std::out_of_range( "No valid level key values found." );
        }
    }

    int GribLoader::editionNumber(const GribField & field) const
    {
        return field.getEditionNumber();
    }

} } } // end namespaces
