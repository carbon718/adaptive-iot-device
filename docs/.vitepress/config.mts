import { defineConfig } from 'vitepress'
import fs from 'node:fs'
import path from 'node:path'

const docsDir = path.resolve(__dirname, '..')

// Automatyczny sidebar: każdy plik NN-*.md w /docs, tytuł z pierwszego nagłówka "# "
const sidebarItems = fs
  .readdirSync(docsDir)
  .filter((f) => /^\d+.*\.md$/.test(f))
  .sort()
  .map((f) => {
    const content = fs.readFileSync(path.join(docsDir, f), 'utf-8')
    const title = content.match(/^#\s+(.+)$/m)?.[1]?.trim() ?? f.replace(/\.md$/, '')
    return { text: title, link: '/' + f.replace(/\.md$/, '') }
  })

export default defineConfig({
  lang: 'pl-PL',
  title: 'Adaptive IoT Device',
  description: 'Wielointerfejsowe urządzenie IoT z adaptacyjnym wyborem technologii komunikacji (LoRa / LTE-M)',
  base: '/adaptive-iot-device/',
  cleanUrls: true,
  lastUpdated: true,
  ignoreDeadLinks: true,
  themeConfig: {
    nav: [
      { text: 'Start', link: '/' },
      { text: 'Dokumentacja', link: sidebarItems[0]?.link ?? '/' }
    ],
    sidebar: [{ text: 'Dokumentacja', items: sidebarItems }],
    socialLinks: [{ icon: 'github', link: 'https://github.com/carbon718/adaptive-iot-device' }],
    search: { provider: 'local' },
    outline: { level: [2, 3], label: 'Na tej stronie' },
    docFooter: { prev: 'Poprzednia', next: 'Następna' },
    lastUpdated: { text: 'Ostatnia aktualizacja' },
    editLink: {
      pattern: 'https://github.com/carbon718/adaptive-iot-device/edit/main/docs/:path',
      text: 'Edytuj na GitHubie'
    },
    darkModeSwitchLabel: 'Motyw',
    sidebarMenuLabel: 'Menu',
    returnToTopLabel: 'Do góry'
  }
})
