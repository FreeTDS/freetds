/** \fn DBCMDROW(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbcmdrow()
 */
/** \fn DBCOUNT(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbcount()
 */
/** \fn DBCURCMD(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbcurcmd()
 */
/** \fn DBCURROW(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbcurrow()
 */
/** \fn DBDEAD(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbdead()
 */
/** \fn DBFIRSTROW(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbfirstrow()
 */
/** \fn DBIORDESC(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro, maps to the internal (lower-case) function.  
 * \sa dbiordesc()
 */
/** \fn DBIOWDESC(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro, maps to the internal (lower-case) function.  
 * \sa dbiowdesc()
 */
/** \fn DBISAVAIL(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbisavail()
 */
/** \fn DBLASTROW(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dblastrow(), DBFIRSTROW()
 */
/** \fn DBMORECMDS(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbmorecmds()
 */
/** \fn DBROWS(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbrows()
 */
/** \fn DBROWTYPE(x)
 * \ingroup dblib_sybase
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa dbrowtype()
 */
/** \fn DBTDS(a)
 * \ingroup dblib_sybase
 * \brief Sybase macro, maps to the internal (lower-case) function.  
 * \sa dbtds()
 */
 /*------------------------*/
/** \fn DBSETLHOST(x,y)
 * \ingroup dblib_api
 * \brief Set the (client) host name in the login packet.  
 * \sa dbsetlhost()
 */
/** \fn DBSETLUSER(x,y)
 * \ingroup dblib_api
 * \brief Set the username in the login packet.  
 * \sa dbsetluser()
 */
/** \fn DBSETLPWD(x,y)
 * \ingroup dblib_api
 * \brief Set the password in the login packet.  
 * \sa dbsetlpwd()
 */
/** \fn DBSETLAPP(x,y)
 * \ingroup dblib_api
 * \brief Set the (client) application name in the login packet.  
 * \sa dbsetlapp()
 */
/** \fn BCP_SETL(x,y)
 * \ingroup dblib_api
 * \brief Enable (or prevent) bcp operations for connections made with a login.  
 * \sa bcp_setl()
 */
 /*------------------------*/
/** \fn DBSETLCHARSET(x,y)
 * \ingroup dblib_sybase
 * \brief Set the client character set in the login packet. 
 * \remark Has no effect on TDS 7.0+ connections.  
 */
/** \fn DBSETLNATLANG(x,y)
 * \ingroup dblib_sybase
 * \brief Set the language the server should use for messages.  
 * \sa dbsetlnatlang(), dbsetlname()
 */
/** \fn dbsetlnatlang(x,y)
 * \ingroup dblib_microsoft
 * \brief Set the language the server should use for messages.  
 * \sa DBSETLNATLANG(), dbsetlname()
 */
/* \fn DBSETLHID(x,y) (not implemented)
 * \ingroup dblib_sybase
 * \brief 
 * \sa dbsetlhid()
 */
/* \fn DBSETLNOSHORT(x,y) (not implemented)
 * \ingroup dblib_sybase
 * \brief 
 * \sa dbsetlnoshort()
 */
/* \fn DBSETLHIER(x,y) (not implemented)
 * \ingroup dblib_sybase
 * \brief 
 * \sa dbsetlhier()
 */
/** \fn DBSETLPACKET(x,y)
 * \ingroup dblib_sybase
 * \brief Set the packet size in the login packet for new connections.  
 * \sa dbsetlpacket(), dbsetllong()
 */
/** \fn dbsetlpacket(x,y)
 * \ingroup dblib_microsoft
 * \brief Set the packet size in the login packet for new connections.  
 * \sa DBSETLPACKET(), dbsetllong()
 */
/** \fn DBSETLENCRYPT(x,y)
 * \ingroup dblib_sybase
 * \brief Enable (or not) network password encryption for Sybase servers version 10.0 or above.
 * \todo Unimplemented.
 * \sa dbsetlencrypt()
 */
/** \fn DBSETLLABELED(x,y)
 * \ingroup dblib_internal
 * \brief Alternative way to set login packet fields.  
 * \sa dbsetllabeled()
 */
/* \fn BCP_SETLABELED(x,y) (not implemented)
 * \ingroup dblib_internal
 * \brief Sybase macro mapping to the Microsoft (lower-case) function.  
 * \sa bcp_setlabeled()
 */
/** \fn DBSETLVERSION(login, version)
 * \ingroup dblib_internal
 * \brief maps to the Microsoft (lower-case) function.  
 * \sa dbsetlversion()
 */

