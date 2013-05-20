lodntoolsdir = @LODN_TOOLS@

dist_lodntools_SCRIPTS = auth.pl view_account_requests.pl lodn_passwd.pl lodn_useradd.pl lodn_usermod.pl lodn_userdel.pl

install-data-hook:
	chmod o-rwx @LODN_TOOLS@/*

clean:
	rm -rf *.pl
