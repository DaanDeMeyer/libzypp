//
//
#warning DO NOT INCLUDE DOXYGEN.h IT CONTAINS JUST DOCUMENTATION STUFF
//
//
////////////////////////////////////////////////////////////////////////////////
/*! \mainpage

\section coding_sec Code snippets
\include g_CodeSnippets

*/
////////////////////////////////////////////////////////////////////////////////
/*! \defgroup g_Resolvable Resolvable Objects
 * \verbinclude g_Resolvable
*/

/*! \defgroup g_ResolvableImplIf Implementation interfaces
 * \ingroup g_Resolvable
 * \verbinclude g_ResolvableImplIf
*/

/*! \defgroup g_ResolvableImpl Implementations
 * \ingroup g_Resolvable
 * \verbinclude g_ResolvableImpl
*/
////////////////////////////////////////////////////////////////////////////////
/*! \defgroup g_EnumerationClass  Enumeration Class
 * \verbinclude g_EnumerationClass
*/
////////////////////////////////////////////////////////////////////////////////
/*! \defgroup g_RAII RAII solutions
 * \see http://en.wikipedia.org/wiki/Resource_Acquisition_Is_Initialization
 * \verbinclude g_RAII
*/
////////////////////////////////////////////////////////////////////////////////
/*! \defgroup g_BackenSpecific Backend Specific
 * \verbinclude g_BackenSpecific
*/
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/*! \defgroup g_Functor Filters and Functors
 * \verbinclude g_Functor
*/
////////////////////////////////////////////////////////////////////////////////
/*! \defgroup g_Algorithm Algorithms
 * \verbinclude g_Algorithm
*/
////////////////////////////////////////////////////////////////////////////////
/** \defgroup BOOST Boost libraries.
 * Boost provides free peer-reviewed portable C++ source libraries.
 * Several \c ::boost names were dragged or typedefed into
 * namespace \c ::zypp.
 * \see http://www.boost.org/
 */
/** \ref BOOST */
namespace boost {}
////////////////////////////////////////////////////////////////////////////////
/** \defgroup SATSOLVER Satsolver interface.
 * Interface to sat-pool and sat-solver.
 */
 namespace zypp 
 {
 	/** \ref SATSOLVER */
 	namespace sat {}
 }

