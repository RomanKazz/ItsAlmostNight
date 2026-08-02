import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Skill Tree Lab — Its Almost Night",
  description: "Интерактивный редактор дерева навыков Its Almost Night.",
  icons: { icon: "/og.png", shortcut: "/og.png" },
  openGraph: {
    title: "Its Almost Night — Skill Tree Lab",
    description: "Интерактивный редактор веток, гибридных нод и зависимостей.",
    images: [{ url: "/og.png", width: 1733, height: 909, alt: "Its Almost Night Skill Tree Lab" }],
  },
  twitter: {
    card: "summary_large_image",
    title: "Its Almost Night — Skill Tree Lab",
    description: "Интерактивный редактор дерева навыков.",
    images: ["/og.png"],
  },
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return <html lang="ru"><body>{children}</body></html>;
}
