# ifndef _FIELDS_H
# define _FIELDS_H

# ifdef HAVE_CONFIG_H
# include "config-ibp.h"
# endif
# include "ibp_base.h"
# include "ibp_protocol.h"

extern IS new_inputstruct(char *filename);
extern void jettison_inputstruct(IS is);
extern int get_line(IS is);

# endif
