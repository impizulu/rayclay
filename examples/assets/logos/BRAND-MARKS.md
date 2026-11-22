# Brand marks

Two marks that are not RayClay's, drawn by ex12_signin_form's sign-in buttons.

## google-g.svg

Google's "G" logo, Google LLC's trademark. Source: the light button sample
`branding_guideline_sample_lt_sq_sl.svg` on the Sign in with Google branding guidelines,
https://developers.google.com/identity/branding-guidelines. The four paths and their colors are
Google's. Changed for RayClay: the button's frame (two `rect` elements) and the `clipPath` group
around the G are removed, since RayClay draws the button and does not support `clipPath`, and the
viewBox is the G's own 20 x 20 box.

## apple-logo.svg (not shipped)

The Apple logo, Apple Inc.'s trademark, is not in this repository: the license that comes with
Apple's artwork does not allow RayClay to redistribute it. To show it, download
`Logo-Sign-in-with-Apple.dmg` from Apple Design Resources (https://developer.apple.com/design/resources/),
take `Sign in with Apple - Left Aligned/SVG/Logo - SIWA - Left-aligned - White - Medium.svg`, remove its
background rectangle, set its fill to `currentColor`, center its 31 x 44 viewBox in a 44 x 44 one
(`rcSvg` draws a square), save it here as `apple-logo.svg`, and build ex12 with `SIGNIN_APPLE_LOGO`
defined to 1. Without it the Apple button shows its label alone.

## Rules

Use these marks only inside a Sign in with Google or a Sign in with Apple button, and only as the
two guidelines above allow: their colors, the button text they list, and never as your own app's
artwork. Apple's artwork comes under the license in Apple's download; read it before you ship.
