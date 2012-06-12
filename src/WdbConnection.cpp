
// project
#include "WdbConnection.hpp"
#include "WciTransactors.hpp"

// libpqxx
#include <pqxx/pqxx>

using namespace pqxx;
using namespace pqxx::prepare;


//SELECT wci.write(273.05, '180', '2012-02-23T00:00:00Z', '2012-02-23T18:00:00Z', '2012-02-23T18:00:00Z', 'air temperature', 'height above ground', 2, 2);
namespace wdb { namespace load { namespace point {

    WdbConnection::WdbConnection(const WdbConfiguration& config, std::string placenamespaceid)
        : pqxx::connection(config.database().pqDatabaseConnection()), config_(new WdbConfiguration(config))
    {
        if(placenamespaceid.empty())
            placenamespaceid = "88";
        perform(WciBegin(config.database().user, 88, boost::lexical_cast<int>(placenamespaceid), 88));
        setup_();
    }

    WdbConnection::~WdbConnection()
    {
        unprepare("WCIWriteFloat");
        unprepare("WciWritePoint");
        unprepare("InfoParameterUnit");
    }

    void WdbConnection::write(
                             const float value,
                             const std::string& placeName,
                             const std::string& referenceTime,
                             const std::string& validTimeFrom,
                             const std::string& validTimeTo,
                             const std::string& valueParameterName,
                             const std::string& levelParameterName,
                             float levelFrom,
                             float levelTo
                            )
    {
        perform(
                WriteFloat(
                           value,
                           placeName,
                           referenceTime,
                           validTimeFrom,
                           validTimeTo,
                           valueParameterName,
                           levelParameterName,
                           levelFrom,
                           levelTo
                          ),
                          1
               );
    }

    void WdbConnection::write(
                             const float value,
                             const std::string& dataProvder,
                             const std::string& placeName,
                             const std::string& referenceTime,
                             const std::string& validTimeFrom,
                             const std::string& validTimeTo,
                             const std::string& valueParameterName,
                             const std::string& levelParameterName,
                             const float levelFrom,
                             const float levelTo,
                             const int dataVersion
                            )
    {
        perform(
                Write(
                       value,
                       dataProvder,
                       placeName,
                       referenceTime,
                       validTimeFrom,
                       validTimeTo,
                       valueParameterName,
                       levelParameterName,
                       levelFrom,
                       levelTo,
                       dataVersion
                     ),
                     1
               );
    }

    void WdbConnection::wcibegin(const std::string& dataProvider)
    {
        perform(wdb::load::point::WciBegin(dataProvider), 1);
    }

    void WdbConnection::setup_()
    {
        prepare("InfoParameterUnit",
                "SELECT * FROM wci.getunit(NULL) WHERE unitname=$1" )
                ("varchar", treat_direct );


        // Statement Insert value
        prepare("WCIWriteFloat",
                "select "
                "wci.write ("
                "$1::float,"
                "$2::text,"
                "$3::timestamp with time zone,"
                "$4::timestamp with time zone,"
                "$5::timestamp with time zone,"
                "$6::text,"
                "$7::text,"
                "$8::real,"
                "$9::real"
                ")" )
                ("float",   treat_direct )
                ("varchar", treat_direct )
                ("varchar", treat_direct )
                ("varchar", treat_direct )
                ("varchar", treat_direct )
                ("varchar", treat_direct )
                ("varchar", treat_direct )
                ("real",    treat_direct )
                ("real",    treat_direct );

        prepare(
                    "WciWritePoint",
                    " select "
                    " wci.write ( "
                    " ROW( "
                    " $1::float,"                         // value
                    " $2::text,"                          // dataprovider
                    " $3::text,"                          // placename
                    " NULL, "                             // placegeometry
                    " $4::timestamp with time zone, "     // referencetime
                    " $5::timestamp with time zone, "     // validtimefrom
                    " $6::timestamp with time zone, "     // validtimeto
                    " 0, "                                // validtimeindeterminatecode (0 - exact)
                    " $7::text, "                         // valueparameter
                    " NULL, "                             // valueparameterunit
                    " $8::text, "                         // levelparameter
                    " NULL, "                             // levelunitname
                    " $9::real, "                         // levefrom
                    " $10::real, "                        // levelto
                    " 0, "                                // levelindeterminatecode
                    " $11, "                              // dataversion -- cant be null
                    " NULL, "                             // confidencecode
                    " CURRENT_TIMESTAMP, "                // storetime
                    " NULL, "                             // valueid
                    " 1 "                                 // valuetype
                    " )::wci.returnFloat"
                    " ) "
               )
                    ("float",   treat_direct ) // value
                    ("varchar", treat_direct ) // dataptovider
                    ("varchar", treat_direct ) // placename
                    ("varchar", treat_direct ) // referencetime
                    ("varchar", treat_direct ) // validtimefrom
                    ("varchar", treat_direct ) // validtimeto
                    ("varchar", treat_direct ) // valueparameter
                    ("varchar", treat_direct ) // levelparameter
                    ("real",    treat_direct ) // levelfrom
                    ("real",    treat_direct ) // levelto
                    ("integer", treat_direct );// dataversion
    }

//" select wci.write ( ROW( 273.182, 'pgen_percentile yr', '87790', NULL, '2012-03-08T12:00:00+00', '2012-03-08T12:00:00+00', '2012-03-10T06:00:00+00', 0, 'air temperature', NULL, 'height above ground', NULL, 2, 2, 0, 43, NULL, CURRENT_TIMESTAMP, NULL, 1)::wci.returnFloat ) "

} } } // end namespaces
