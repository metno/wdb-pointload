pkgsysconf_DATA = 	etc/loadfelt.cfg \
                        etc/loadfelteps.cfg \
                        etc/loadgrib.cfg \
                        etc/loadgribeps.cfg \
                        etc/test.nc \

                        etc/felt/fimexreader.xml \
                        etc/felt/dataprovider.conf \
		        etc/felt/validtime.conf \
			etc/felt/valueparameter.conf \
			etc/felt/levelparameter.conf \
			etc/felt/leveladditions.conf \

                        etc/felteps/felt2nc_vars_eps.xml \
                        etc/felteps/felt_axes.xml \
                        etc/felteps/felt_global_attributes.xml \
                        etc/felteps/felt_variables.xml \
                        etc/felteps/dataprovider.conf \
		        etc/felteps/validtime.conf \
			etc/felteps/valueparameter.conf \
			etc/felteps/levelparameter.conf \
			etc/felteps/leveladditions.conf \

                        etc/grib/fimexreader.xml \
                        etc/grib/dataprovider.conf \
			etc/grib/valueparameter1.conf \
			etc/grib/levelparameter1.conf \
			etc/grib/leveladditions1.conf \
			etc/grib/valueparameter2.conf \
			etc/grib/levelparameter2.conf \
			etc/grib/leveladditions2.conf

EXTRA_DIST += $(pkgsysconf_DATA)
