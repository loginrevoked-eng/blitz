<?php
/**
 * The base configuration for WordPress
 *
 * The wp-config.php creation script uses this file during the installation.
 * You don't have to use the web site, you can copy this file to "wp-config.php"
 * and fill in the values.
 *
 * This file contains the following configurations:
 *
 * * Database settings
 * * Secret keys
 * * Database table prefix
 * * ABSPATH
 *
 * @link https://wordpress.org/support/article/editing-wp-config-php/
 *
 * @package WordPress
 */

// ** Database settings - You can get this info from your web host ** //
/** The name of the database for WordPress */
define( 'DB_NAME', 'eletrogenesis_db' );

/** Database username */
define( 'DB_USER', 'eletrogenesis_user' );

/** Database password */
define( 'DB_PASSWORD', 'eletrogenesis_pw' );

/** Database hostname */
define( 'DB_HOST', 'localhost' );

/** Database charset to use in creating database tables. */
define( 'DB_CHARSET', 'utf8' );

/** The database collate type. Don't change this if in doubt. */
define( 'DB_COLLATE', '' );

/**#@+
 * Authentication unique keys and salts.
 *
 * Change these to different unique phrases! You can generate these using
 * the {@link https://api.wordpress.org/secret-key/1.1/salt/ WordPress.org secret-key service}.
 *
 * You can change these at any point in time to invalidate all existing cookies.
 * This will force all users to have to log in again.
 *
 * @since 2.6.0
 */
define( 'AUTH_KEY',         'scsG7&1}96(6Cp4@V#A+,+sN7^`c/58O{hCMp6^o!K|`[JTYV}FuU0q3s9(GtWAP' );
define( 'SECURE_AUTH_KEY',  'JHk0ZQ+D[)<gn%/TPM0h/.E^/mw$V1=+#lZD7i]RXyvE_- I_*p#I6y2!ga[kaUq' );
define( 'LOGGED_IN_KEY',    '}olDt8%e *cZW[W<*cRNlb5ZT.Q*Vc9=$SjKX<1O4xb1<Mbl)[OAK2;NIRJ+gR&$' );
define( 'NONCE_KEY',        '@+zi?6KIK}o22mv[7qrj3~#Z9wE/9!p:Fb~,*{R.kA7<5k!o*1XSHI2=0ss:{A&F' );
define( 'AUTH_SALT',        '/6;z;t4dWV(pKZ9z~($fMC+  %&{87]Qvpf: 3I)~QnW1Sq7fb+T9h8NQI:KkJyN' );
define( 'SECURE_AUTH_SALT', 'gb}vM!0fDGJ=/.+G5Y< bZ|q<tICuBcIQ7VQNR-`;KGbhc0.@j}olCU[dxOC<*$,' );
define( 'LOGGED_IN_SALT',   'g,XgNdL[clRC.nRqV|Qp/T*VC.<{)$s@D)+.?j2-|oCOa<-.Lw?($],tLtnE(;a6' );
define( 'NONCE_SALT',       'Djr:zGx[J,;o|yTUfm<{<P[Ud4;fkTT`Vqn-::vX[ltk4@iqvOL.x.5[exm>rA&%' );
/**#@-*/

/**
 * WordPress database table prefix.
 *
 * You can have multiple installations in one database if you give each
 * a unique prefix. Only numbers, letters, and underscores please!
 */
$table_prefix = 'wp_';

/**
 * For developers: WordPress debugging mode.
 *
 * Change this to true to enable the display of notices during development.
 * It is strongly recommended that plugin and theme developers use WP_DEBUG
 * in their development environments.
 *
 * For information on other constants that can be used for debugging,
 * visit the documentation.
 *
 * @link https://wordpress.org/support/article/debugging-in-wordpress/
 */
define( 'WP_DEBUG', false );

/* Add any custom values between this line and the "stop editing" line. */



/* That's all, stop editing! Happy publishing. */

/** Absolute path to the WordPress directory. */
if ( ! defined( 'ABSPATH' ) ) {
	define( 'ABSPATH', __DIR__ . '/' );
}

/** Sets up WordPress vars and included files. */
require_once ABSPATH . 'wp-settings.php';
