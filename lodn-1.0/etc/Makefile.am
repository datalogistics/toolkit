lodnetcdir = @LODN_ETC@

dist_lodnetc_DATA = lodn.cfg userinfo publishlist account_requests

install-data-hook:
	chmod o-r @LODN_ETC@/*

userinfo:
	@PERL_BIN@ ./make_storable_file.pl userinfo

publishlist:
	touch publishlist

account_requests:
	touch account_requests

clean:
	rm -rf lodn.cfg userinfo publishlist account_requests
