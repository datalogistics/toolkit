SUBDIRS = images 

lodnwebdir = @LODN_WEB@

dist_lodnweb_DATA = LoCI_logo.html LoDN_Burn.php autologout.html burning.php create_account.html footer.html get-javaws.html index.html lodn.css style.css warmer_watch.html

install-data-hook:
	chmod o-rx @LODN_WEB@/*.html @LODN_WEB@/*.php @LODN_WEB@/*.css
