   
#     Copyright (C) 2010  Christopher Brumgard
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
#
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.



import os
import os.path
import sys
import stat
import glob
import getpass
import random
import re
import shutil
import subprocess
import zipfile
from collections import namedtuple
from optparse import OptionParser
from distutils.core import setup
import configparser

    


def parseArgs():
    
    parser = OptionParser(usage = "usage: python %prog install [options]")
    
    parser.add_option("--prefix", dest="prefix", default=os.getcwd(),
                      help="Base directory for installation", metavar="path")
    parser.add_option('--depotfile', dest='depotfile', default=None,
                      help='Depot descripton file', metavar='file')
    parser.add_option('--with-mysql', dest='mysql', default=None,
                      help='Path to the mysql installations', 
                      metavar='path-to-mysql')
    parser.add_option('--clear-database', dest='clear_database', default=False,
                      action='store_true',
                      help='Clears the database of previous tables')
    parser.add_option('--storcore', dest='storcore', default=None,
                      help='storecore web server', metavar='host:port')
    parser.add_option('--use-previous-settings', dest='use_previous_settings', 
                      action='store_true', default=False,
                      help='Uses the current settings instead of asking for settings')

    options, args = parser.parse_args()
    
    if 'install' not in args:
        sys.exit("Forgot to specify install on the command line") 
    
    return options, args
   



def generateDistutilFile(base, lib=None, scripts=None, data=None):
    
    # Sets lib, scripts and data to the default values if they are not specified
    if lib is None: lib = os.path.join(base, 'lib')
    if scripts is None: scripts = os.path.join(base, 'bin')
    if data is None: data  = os.path.join(base, 'etc')
    
    # Format string for message 
    formatStr = ("[install]\ninstall-base={base}\ninstall-purelib={lib}\n" +
                 "install-headers=headers\ninstall-platlib={lib}\ninstall-scripts={scripts}\n"      +
                 "install-data={data}\n")

    # Generates the setup configuration file 
    with open('setup.cfg', 'w') as f:
        print(formatStr.format(base=base, lib=lib, scripts=scripts, data=data), 
              file=f)

def askQuestion(question, default):
    return True and input(question) or default



def getPythonData(configData, oldConfigData=dict()):
    
    if 'python' in oldConfigData:
        defaultPythonPath  = oldConfigData['python'].get('python_path', '')
        defautLibraryPath  = oldConfigData['python'].get('library_path', '')
    
    else:
        defaultPythonPath  = ''
        defautLibraryPath  = ''
    
    
    configData['python'] = dict()
    configData['python']['python_path']  = defaultPythonPath
    configData['python']['library_path'] = defautLibraryPath
    
    
def getProtocolBufferData(configData, oldConfigData=dict()):
    
    # gets the starting data
    if 'pb_interface' in oldConfigData:
        defaultHost         = oldConfigData['pb_interface'].get('host', 'localhost')
        defaultPort         = oldConfigData['pb_interface'].get('port', '5000')
        
    else:
        defaultHost         = 'localhost'
        defaultPort         = '5000'
        
    # Initialize the protocol buffer section of config data    
    configData['pb_interface'] = dict()
    
    print("Please answer the following questions about the protocol buffer.")
    
    # Gets the host
    configData['pb_interface']['host'] = askQuestion(
                "  What is the host to serve on? [{0}] ".format(defaultHost), 
                defaultHost)
    
    # Gets the port
    configData['pb_interface']['port'] = int(askQuestion(
                "  What is the port to server on? [{0}]".format(defaultPort), 
                defaultPort))
    
    
def getDataDispatcherData(configData, oldConfigData=dict()):
    
    # Gets the starting data
    if 'data_dispatcher' in oldConfigData:
        max_connections  = oldConfigData['data_dispatcher'].get(
                                                        'max_connections', 
                                                        '1')
        
        timeout          = oldConfigData['data_dispatcher'].get(
                                                        'timeout', 
                                                        '30')
        
        initial_duration = oldConfigData['data_dispatcher'].get(
                                                        'initial_duration', 
                                                        '3600')
        augment_interval = oldConfigData['data_dispatcher'].get(
                                                        'augment_interval',
                                                        '3600')
    else:
        max_connections  = '64'
        timeout          = '60'
        initial_duration = '3600'
        augment_interval = '3600'
        
    # Initialize the protocol buffer section of config data    
    configData['data_dispatcher'] = dict()
    
    print("Please answer the following questions about the data dispatcher.")
      
    # Gets the max number of connections
    configData['data_dispatcher']['max_connections'] = askQuestion(
                "  What is the maximum number of connections? [{0}] ".format(max_connections), 
                max_connections)
    
    # Gets the default timeout
    configData['data_dispatcher']['timeout'] = askQuestion(
                "  What default timeout to use for operations? [{0}] ".format(timeout), 
               timeout)
    
    # Gets the initial allocation duration
    configData['data_dispatcher']['initial_duration'] = askQuestion(
                "  What is the duration of an allocation? [{0}] ".format(initial_duration), 
                initial_duration)
    
    # Gets the initial allocation duration
    configData['data_dispatcher']['augment_interval'] = askQuestion(
                "  What is the augment interval? [{0}] ".format(augment_interval), 
                augment_interval)
    
        
def getMySQLData(configData, oldConfigData=dict()):
    
    
    # Gets the default values to use
    if 'mysql' in oldConfigData:
        defaultSocket        = oldConfigData['mysql'].get('unix_socket', '')
        defaultHost          = oldConfigData['mysql'].get('host', 'localhost')
        defaultPort          = oldConfigData['mysql'].get('port', '3306')
        defaultAccountName   = oldConfigData['mysql'].get('account', '')
        defaultAccountPasswd = oldConfigData['mysql'].get('passwd', '')
        defaultDBName        = oldConfigData['mysql'].get('dbname', '')
    else:
        defaultSocket        = ''
        defaultHost          = 'localhost'
        defaultPort          = '3306'
        defaultAccountName   = ''
        defaultAccountPasswd = ''
        defaultDBName        = ''
        
    
    # Initialize the mysql section of config data
    configData['mysql'] = dict()
    
    
    
    print("Please answer the following questions about the MySQL server.")
    
    ans = askQuestion("  Is the MySQL server on the same machine as LoDN? [Y/n] ", 
                      'y').lower()

    # Local Machine
    if ans == 'y':
        
        if defaultSocket != '':
            ans = askQuestion("  Does MySQL server support Unix domain sockets? [Y/n] ", 
                          'y').lower()
                          
        else:
            ans = askQuestion("  Does MySQL server support Unix domain sockets? [y/N] ", 
                          'n').lower()
        
        
        # Unix domain socket
        if ans == 'y':
            configData['mysql']['unix_socket'] = askQuestion(
                                "  Enter the Unix domain socket: [{0}] ".format(
                                        defaultSocket), defaultSocket).lower()
            
            if len(configData['mysql']['unix_socket']) == 0: 
                sys.exit("Invalid response3")
        
        # Use inet socket
        elif ans == 'n':
            
            configData['mysql']['host'] = askQuestion(
                    "  Enter the hostname of the MySQL server: [{0}] ".format(
                                    defaultHost), defaultHost)
            
            
            configData['mysql']['port'] = int(askQuestion(
                    "  Enter the port of the MySQL server: [{0}] ".format(
                                    defaultPort), defaultPort))
           
        else:
            sys.exit("Invalid response1")
      
    # Not a local machine  
    elif ans == 'n':
        
        configData['mysql']['host'] = askQuestion(
                "  Enter the hostname of the MySQL server: [{0}] ".format(
                                defaultHost), defaultHost)
        
        
        configData['mysql']['port'] = askQuestion(
                "  Enter the port of the MySQL server: [{0}] ".format(
                                defaultPort), defaultPort)
            
    else: 
        sys.exit("Invalid response2")
    
    
    # Gets the Account name for use with MySQL
    configData['mysql']['account']    = askQuestion(
        "  Enter MySQL Account Name: [{0}] ".format(defaultAccountName), 
        defaultAccountName)
    if configData['mysql']['account'] == "": sys.exit("Invalid response")
    
    # Gets the Password for the MySQL account
    configData['mysql']['passwd']     = getpass.getpass(
        "  Enter MySQL Account Password: [{0}] ".format(defaultAccountPasswd))

    if configData['mysql']['passwd'] == "":
        if defaultAccountPasswd != "":
            configData['mysql']['passwd'] = defaultAccountPasswd
        else:
            sys.exit("Invalid response")
    
    # Gets the database name
    configData['mysql']['dbname']     = askQuestion(
        "  Enter MySQL Database Name: [{0}] ".format(defaultDBName), 
        defaultDBName)
    if configData['mysql']['dbname'] == "": sys.exit("Invalid response")
    
    
    
    
def getAdminData(configData):
    
    configData['AdminPasswd']     = getpass.getpass("  LoDN Admin Password: ")
    if configData['AdminPasswd'] == "": sys.exit("Invalid response")
    
    
def generateConfigurationFile(configData, file='lodn.cfg'):
    
    try:
        
        # Changes the mask so only the user can read/write and the group
        # can read
        oldMask = os.umask(stat.S_IXGRP|stat.S_IWGRP|stat.S_IRWXO)
        
        # Creates a parser
        parser = configparser.RawConfigParser()
        
        # Iterates over the sections adding the fields values
        for section, sectionDict in configData.items():
            
            parser.add_section(section)
            
            for field, value in sectionDict.items():
                parser.set(section, field, value)
        
        # Writes the results to file
        parser.write(open(file, 'w'))
        
        # Restores the umask
        os.umask(oldMask)
    
    # Catches any error
    except Exception as e:
        sys.exit("Error writing "+file+" due to "+str(e))
    
    

def readConfigFile(configData, file='lodn.cfg'):
    

    try:
        
        # Creates a parser to read old file
        parser = configparser.RawConfigParser()
    
        # Reads the file
        parser.read(file)
    
        # Iteraters over the sections storing the configuration data
        for section in parser.sections():
        
            if section not in configData:
                configData[section] = dict()
        
            for key, value in parser.items(section):
                configData[section][key] = value
    
    # Catches any error
    except Exception as e:
        sys.exit("Error reading config file %s: " + str(e))
    
    

  
def clearMysqlDatabase():
    
    '''clearMysqlDatabase() --> None 
    
    This function clears the mysql database of all tables, functions and
    procedures.'''
    
    
    
    from lodn.server import globals
    import pyMySQL

    
    try:
        
        # Gets a connection and cursor to the database
        with globals.dbManager.getConnection() as conn:
            
            print('Removing tables...' )
            
            cursor = conn.cursor()
            
            # Gets the tables
            cursor.execute('SHOW TABLES')
            
            # Counts and builds a list of the tables
            numTables = cursor.rowcount
            tables = [ row[0] for row in cursor.fetchall() ]
            
            # Keeps trying to remove the tables until there are none left
            while numTables > 0:
                
                # Drops the tables
                try:
                    cursor.execute('DROP TABLE {}'.format(', '.join(tables)))
                
                # Catches when a table can't be dropped because another
                # table depends on it
                except pyMySQL.Error as e:
                    if e.args[0] != 1217:
                        raise e
                    
                # Gets a list of the remainder tables and shuffles them around
                # for dependencies
                cursor.execute('SHOW TABLES')
                numTables = cursor.rowcount
                tables = random.sample([ row[0] for row in cursor.fetchall() ], 
                                       numTables)
                
            print('Removing functions...')
            
            # Gets the functions
            cursor.execute('SHOW FUNCTION STATUS')
            functions = [ row[1] for row in cursor.fetchall() ]
            
            # Removes the functions
            for function in functions:
                try:
                    cursor.execute('DROP FUNCTION {}'.format(function))
                except pyMySQL.Error:
                    pass
            
            print('Removing procedures...')
            
            # Gets the procedures
            cursor.execute('SHOW PROCEDURE STATUS')
            procedures = [ row[1] for row in cursor.fetchall() ]
            
            # Removes the procedures
            for procedure in procedures:
                try:
                    cursor.execute('DROP PROCEDURE {}'.format(procedure))
                except pyMySQL.Error:
                    pass
            
            
    # Catches pyMySQL errors
    except pyMySQL.Error as e:
        sys.exit(e)
        
  
def initializeMySQL(configData, options):

    mysqlArgs = dict()
    mysqlArgs['username']  = configData['mysql']['account']
    mysqlArgs['passwd']    = configData['mysql']['passwd']
    mysqlArgs['db_name']   = configData['mysql']['dbname']
    
    if 'unix_socket' in configData['mysql']:
        mysqlArgs['unix_socket'] = configData['mysql']['unix_socket']
        
    else:
        mysqlArgs['host'] = configData['mysql']['host']
        mysqlArgs['port'] = configData['mysql']['port']


    import pyMySQL
    from lodn.server import globals
    from lodn.server.db import managers
    from lodn.server import error
    


    try:
     
        # Create db manager for the process
        manager = managers.ThreadConnectionManager(pyMySQL, **mysqlArgs)

        globals.dbManager = manager
    
        # Clears the mysql database
        if options.clear_database:
            clearMysqlDatabase()


        # Initialize the errors
        error.initialize()
       
        
        from lodn.server.subsystems import fs
        from lodn.server.subsystems import data_dispatcher
        
     
        # Query the depots
        depots = fs.queryDepots(storcore=options.storcore, 
                                depotfile=options.depotfile)
        
        print("Using these resources:")
        for host, port, rid, site, coordinates in depots:
            print('\t{}:{}:{} at {} with coordinates {} {}'.format(
                        host, port, rid, site, coordinates[0], coordinates[1]))
        
        # Initialize the filesystem
        fs.initialize(path=os.path.normpath(fs.__file__+"/../../../"), 
                      depots=depots)
        
        # Initialize the data dispatcher
        data_dispatcher.initialize(
                    duration=configData['data_dispatcher']['initial_duration'], 
                    timeout=configData['data_dispatcher']['timeout'])
        
        
    
    except pyMySQL.Error as e:
        sys.exit("Aborting, Error with MySQL Server: " + e.args[1])
            
        
def produceFiles(files=[], keywordValues=dict()):
    
    try:
        for file in files:
            
            print("Producing", file, "...")
            
            templatefile = file + '.in'
            
            with open(templatefile, 'r') as input:
                data = input.read()
        
            for key, value in keywordValues.items():
                data = data.replace('@{keyword}@'.format(keyword=key), value)
        
            with open(file, 'w') as output:
                output.write(data)
            
    except Exception as e:
        sys.exit("Error producing files: "+str(e))
        
        
def getSystemType():
    
    unameTuple = os.uname()
        
    return unameTuple[0], unameTuple[4]




def buildpyMySQL(mysqlLocation):

    # Unzip the pyMySQL zip file
    zipfile.ZipFile('lib/pyMySQL.zip').extractall(path='lib')

    # Change to the directory of the src code for pyMySQL
    os.chdir('lib/pyMySQL/src')
    
    # Builds the arg list
    buildArgs = [sys.executable, 'pyMySQL_setup.py', 'build']
    
    if mysqlLocation is not None:
        buildArgs.append('--with-mysql=' + mysqlLocation)
    
    # Attempts to build the pyMySQL module
    retcode = subprocess.call(buildArgs)

    if retcode != 0:
        sys.exit('Error building pyMySQL C extension module')
        
    # Gets the filename of library
    libfiles = glob.glob('build/lib.*/pyMySQL.so')
        
    # Checks the file exists
    if len(libfiles) == 0:
        sys.exit('Error building pyMySQL C extension module')
        
    # Copies the file to the lib directory
    shutil.copyfile(libfiles[0], '../../pyMySQL.so')
    shutil.copymode(libfiles[0], '../../pyMySQL.so')

    # Return to the original directory
    os.chdir('../../../')
    
    
    
if __name__ == "__main__":
    

    # Check the version of python
    if sys.version_info[0] < 3 or (sys.version_info[0] == 3 and 
                                   sys.version_info[1] < 1):
        sys.exit("Error: Python version must be >= 3.1")
        
    # Parses the args
    options, args = parseArgs()
    
  
    # Gets the system and architecture type
    system, arch = getSystemType()
    
    print('Machine detected as {} on {} architecture'.format(system, arch))


    configData    = dict()
    oldConfigData = dict()

    
    # If the old lodn.cfg file exists it read the data from that
    if os.access('lodn.cfg', os.F_OK|os.R_OK):
        readConfigFile(oldConfigData)

    # Check path to Python and use it in scripts
    python_exe  = sys.executable
    python_path = str([ options.prefix + '/lib' ])
    
    if options.use_previous_settings:
        
        if len(oldConfigData) == 0:
            sys.exit('No lodn.cfg file')
        
        configData = oldConfigData
        
    else:
        
        getPythonData(configData, oldConfigData)
        
        # Gets the protocol buffer data
        getProtocolBufferData(configData, oldConfigData)
        
        # Gets the data dispatcher data
        getDataDispatcherData(configData, oldConfigData)
        
        # Queries the users for MySQL Data
        getMySQLData(configData, oldConfigData)
        
    # Queries the user for admin data for LoDN
    #getAdminData(configData)
    

    # Adds paths to the python search path
    sys.path.extend(configData.get('python',dict()).get('python_path',':').split(':') +
                    ['lib'] +
                    glob.glob('lib/LoDN*.egg'))
    


    os.environ['LD_LIBRARY_PATH'] = configData.get('python',dict()).get('library_path', '')
    os.putenv('LD_LIBRARY_PATH', configData.get('python',dict()).get('library_path', ''))
    
    configData['lodn'] = dict()
    configData['lodn']['var'] = os.path.normpath(os.path.join(options.prefix, 'var'))
    
    
    # Produces the configuration data
    generateConfigurationFile(configData)
    
    # Build the python module
    buildpyMySQL(options.mysql)
    
    # Initialize MySQL 
    initializeMySQL(configData, options)
    
    
    # Generate the configuation file
    generateDistutilFile(options.prefix)
    
    produceFiles(['./scripts/install_policy.py',
                  './scripts/register_depot.py',
                  './scripts/lodn_shutdown.py',
                  './scripts/lodnd.py'], 
                                            {'python_exe' : python_exe,
                                             'python_path' : python_path,
                                             'prefix' : options.prefix,
                                             'var' : os.path.normpath(os.path.join(
                                                    options.prefix, 'var'))})
    
    # Import the Version info now
    from lodn.server.version import getVersion
    
    # Run setup
    setup(name="LoDN", 
          version=getVersion(),
          description="LoDN",
          author="Christopher Brumgard",
          author_email="sellers@cs.utk.edu",
          url="loci.eecs.utk.edu",
          #packages = ['dist/LoDN-1.5.0.Winnipeg.0-py3.1.egg'],
          #package_dir = { 'dist' : 'lib'  },
          #packages    = { 'lodn', 'lodn.server', 'lodn.server.error', 
          #                'lodn.server.event', 
          #                'lodn.client', 'lodn.protocol', 
          #                'lodn.server.subsystems',
          #                'lodn.server.subsystems.fs',
          #                'lodn.server.subsystems.data_dispatcher', 
          #                'lodn.server.ibp',
          #                'lodn.server.db' },
          #data_files = [('', ['lodn.cfg']), ('mysql', glob.iglob('lodn/server/mysql/*.sql'))],
          data_files = [ ('', ['lodn.cfg']), 
                         ('../lib', glob.glob('lib/*.egg') + 
                                    glob.glob('lib/*py') + 
                                    glob.glob('lib/*.zip') + 
                                    glob.glob('lib/*.so')),
                         ('../var', [])
                       ],
          #scripts=['lodn/scripts/lodn_admin.py', 'lodn/scripts/install_policy.py'],
          scripts=['./scripts/install_policy.py', 
                   './scripts/register_depot.py',
                   './scripts/lodnd.py',
                   './scripts/lodn_shutdown.py'],
          script_args = args)
    
    
