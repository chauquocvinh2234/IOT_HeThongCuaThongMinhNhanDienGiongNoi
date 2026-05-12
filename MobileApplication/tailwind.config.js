/** @type {import('tailwindcss').Config} */
module.exports = {
  // NOTE: Update this to include the paths to all of your component files.
  content: ["./App.{js,jsx,ts,tsx}", "./src/**/*.{js,jsx,ts,tsx}"],
  presets: [require("nativewind/preset")],
  theme: {
    extend: {
      colors: {
        primary: '#6C63FF',
        primaryLight: '#8B83FF',
        secondary: '#00D9A6',
        accent: '#FF6B8A',
        warning: '#FFB84D',
        background: '#0F0F1A',
        card: '#1A1A2E',
        cardLight: '#222240',
        text: '#FFFFFF',
        textSecondary: '#8A8AA3',
        border: '#2A2A4A',
      }
    },
  },
  plugins: [],
}
