lodnwebimagedir = @LODN_WEB@/images

dist_lodnwebimage_DATA = IBPvo_button.gif LoCI_Title.gif bgstripe.gif bottomleft_corner.gif bottomleft_corner_light.gif bottomright_corner.gif bottomright_corner_light.gif citr.gif deleted.gif doe.gif down.gif first-grey.gif folder.gif green.gif last-grey.gif last.gif linux_button.gif lodn_mail.gif logo_01.gif logo_02.gif logout.gif next-grey.gif next.gif nsf.gif pixel.gif planetlab.gif prev-grey.gif red.gif sinrg.gif text.gif topleft_corner.gif topright_corner.gif up.gif ut.gif utk.gif apple.png broken.png button_browse.png button_drop.png button_edit.png button_fulltext.png button_index.png button_insert.png button_nofulltext.png button_primary.png button_properties.png button_recycle.png button_rename.png button_ro.png button_unique.png button_view.png clock.png danger.png dead.png folder-home.png good.png lociwords.png new.png recycle.png renfolder.png upfolder.png lodn_logo64.jpg favicon.ico ut1.gif vubw.jpg reddnet-25.jpg reddnet-26.jpg reddnet-27.png powered-by-reddnet.png plus.png

lodn_mail.gif:
	if [ "x@CONVERT_BIN@" != "x" ]; then @CONVERT_BIN@ -background white -fill black -pointsize 16 label:"@ADMIN_EMAIL@" lodn_mail.gif; else cp lodn_mail_blank.gif lodn_mail.gif;  fi

install-data-hook:
	chmod o-rx @LODN_WEB@/images/*
