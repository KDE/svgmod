/****************************************************************************
 *   Copyright (C) 2011 by Manuel Mommertz <2kmm@gmx.de>                    *
 *                                                                          *
 *   This program is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation, either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   This program is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ****************************************************************************/

#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QRegExp>
#include <QtGui/QColor>
#include <QtXml/QDomDocument>
#include <iostream>

#define VERSION "0.3"

enum {
  TO_FEW_ARGUMENTS = 1,
  NO_FILES_PROVIDED,
  NO_COMMANDS_PROVIDED,
  OPENING_R_FAILED,
  OPENING_W_FAILED,
  PARSING_FAILED,
  WRITING_FAILED,
  CREATING_BACKUP_FAILED,
  APPLYING_CLASS_FAILED
};

enum Cmd {
  addClass,
  colorToClass,
  reapply
};

struct Command {
  Cmd cmd;
  QList<QString> arg;
};

bool checkCommand(const QString &cmd, int needed, int pos, int available)
{
  if (pos + needed < available) {
    return true;
  }
  std::wcerr << "Command '" << cmd.toStdWString() << "' requires " <<
    needed << " argument" << (needed == 1 ? "." : "s.") << std::endl;
  return false;
}

void displayVersion()
{
  std::cout << VERSION << std::endl;
}

void displayHelp()
{
  std::cout << "Usage: svgmod <COMMAND [OPTIONS]>... FILE...\n\n";
  std::cout << "Commands:\n";
  std::cout << "  -h, --help\n";
  std::cout << "         display this help\n\n";
  std::cout << "  -v, --version\n";
  std::cout << "         display program version\n\n";
  std::cout << "  -b, --backup POSTFIX\n";
  std::cout << "         creates a backup of files with POSTFIX appended to their name\n\n";
  std::cout << "  -a, --add-class STYLE_ID NAME COLOR\n";
  std::cout << "         add a new class to the style with id STYLE_ID\n\n";
  std::cout << "  -c, --color-to-class COLOR CLASS\n";
  std::cout << "         find elements with color COLOR, remove the color and apply the class CLASS to the element\n\n";
  std::cout << "  -r, --reapply\n";
  std::cout << "         remove color-attributes from all elements (try this after editing your file with inkscape)\n\n";
  std::cout << std::endl;
}

QColor stringToColor(const QString &string)
{
  QRegExp reColor("rgb\\(\\s*([0-9]+)(%?)\\s*,\\s*([0-9]+)(%?)\\s*,\\s*([0-9]+)(%?)\\s*\\)");
  if (reColor.exactMatch(string)) {
    int r = reColor.cap(1).toInt();
    if (reColor.cap(2) == "%") {
      r = r * 255 / 100;
    }
    int g = reColor.cap(3).toInt();
    if (reColor.cap(4) == "%") {
      g = g * 255 / 100;
    }
    int b = reColor.cap(5).toInt();
    if (reColor.cap(6) == "%") {
      b = b * 255 / 100;
    }
    return QColor(r, g, b);
  } else if (QColor::isValidColor(string)) {
    return QColor(string);
  } else {
    return QColor();
  }
}

void cmd_addClass(QDomDocument &svg, const QString &id, const QString &name, const QColor &color)
{
  QDomNodeList list(svg.elementsByTagName("style"));
  QDomElement style;
  for (int i = 0; i < list.length(); ++i) {
    QDomElement cur = list.at(i).toElement();
    if (cur.attribute("id", "") == id) {
      style = cur;
      break;
    }
  }
  if (style.isNull()) {
    QDomElement defs = svg.elementsByTagName("defs").item(0).toElement();
    if (defs.isNull()) {
      defs = svg.createElement("defs");
      svg.documentElement().insertBefore(defs, QDomNode());
    }
    style = svg.createElement("style");
    style.setAttribute("type", "text/css");
    if (!id.isEmpty()) {
      style.setAttribute("id", id);
    }
    defs.insertBefore(style, QDomNode());
  }
  list = style.childNodes();
  QRegExp reClass("(?:^|;|\\})\\s*\\." + name + "\\s*\\{");
  QRegExp reBrace("\\{|\\}");
  QRegExp reColor("(?:^|;)\\s*color\\s*:\\s*([^;]+)\\s*(?:$|;)");
  for (int i = 0; i < list.length(); ++i) {
    QDomNode node = list.at(i);
    QString text = node.nodeValue();
    if (text.isEmpty()) {
      continue;
    }
    int pos = 0;
    while (reClass.indexIn(text, pos, QRegExp::CaretAtOffset) >= 0) {
      pos = reClass.pos() + reClass.matchedLength();
      int level = 0;
      while (reBrace.indexIn(text, pos, QRegExp::CaretAtOffset) >= 0) {
        if (level == 0) {
          if (reColor.indexIn(text, pos, QRegExp::CaretAtOffset) >= 0 &&
              reColor.pos() < reBrace.pos()) {
            text.replace(reColor.pos(1), reColor.cap(1).length(), color.name());
            node.setNodeValue(text);
            return;
          }
        }
        pos = reBrace.pos() + reBrace.matchedLength();
        if (reBrace.cap(0) == "{") {
          ++level;
        } else if (reBrace.cap(0) == "}") {
          if(level == 0) {
            break;
          }
          --level;
        }
      }
    }
  }
  QDomNode node;
  QString data = "\n." + name + "{ color:" + color.name() + "; }\n";
  if (list.length()) {
    node = list.at(0);
    node.setNodeValue(node.nodeValue() + data);
  } else {
    node = svg.createCDATASection(data);
    style.insertBefore(node, QDomNode());
  }
}

bool applyClassToElement(QDomDocument &svg, QDomElement element, const QString &classname)
{
  element.removeAttribute("color");
  QString style = element.attribute("style");
  QRegExp reColor("(^|;)\\s*color\\s*:\\s*[^;]+\\s*(?:$|;)");
  if (reColor.indexIn(style) >= 0) {
    style.replace(reColor.pos(0), reColor.cap(0).length(), reColor.cap(1));
    element.setAttribute("style", style);
  }
  QRegExp reClass("(^| )" + classname + "(?:$| )");
  if (element.tagName() == "stop") {
    QDomElement gradient;
    for (gradient = element.parentNode().toElement();
         !gradient.isNull() &&
         gradient.tagName() != "linearGradient" && gradient.tagName() != "radialGradient";
         gradient = gradient.parentNode().toElement());
    if (gradient.isNull()) {
      if (element.lineNumber() >= 0) {
        std::cerr << "Line " << element.lineNumber() << ":\n";
      }
      std::cerr << "Error: Could not find the right gradient for this 'stop'-tag." << std::endl;
      return false;
    }
    QDomElement group = gradient.parentNode().toElement();
    if (group.tagName() == "g") {
      if (!group.attribute("class").isEmpty()) {
        if (reClass.indexIn(group.attribute("class")) >= 0) {
          return true;
        }
        if (element.lineNumber() >= 0) {
          std::cerr << "Line " << element.lineNumber() << ":\n";
        }
        std::cerr << "Error: You are using more then one class in a gradient.\n";
        std::cerr << "       This is currently not supported.\n";
        std::cerr << "Class in use: " << group.attribute("class").toStdString() << "\n";
        std::cerr << "Class to apply: " << classname.toStdString() << std::endl;
        return false;
      }
      element = group;
    } else {
      group = svg.createElement("g");
      gradient.parentNode().insertBefore(group, gradient);
      group.appendChild(gradient);
      element = group;
    }
  } else {
    if (reClass.indexIn(element.attribute("class")) >= 0) {
      return true;
    }
    if (!element.attribute("class").isEmpty()) {
      QDomElement group = svg.createElement("g");
      group.setAttribute("class", element.attribute("class"));
      element.parentNode().insertBefore(group, element);
      group.appendChild(element);
      if (element.attribute("fill") == "currentColor") {
        group.setAttribute("fill", "currentColor");
        element.removeAttribute("fill");
      }
      if (element.attribute("stroke") == "currentColor") {
        group.setAttribute("stroke", "currentColor");
        element.removeAttribute("stroke");
      }
      QRegExp reAttr("(?:^|;)\\s*([^:;]+)(\\s*:\\s*currentColor\\s*(?:$|;))");
      int pos=0;
      while ((pos = reAttr.indexIn(style, pos, QRegExp::CaretAtOffset)) >= 0) {
        if (reAttr.cap(1) == "fill" || reAttr.cap(1) == "stroke") {
          style.remove(reAttr.pos(1), reAttr.cap(1).length() + reAttr.cap(2).length());
          element.setAttribute("style", style);
          group.setAttribute("style", group.attribute("style") + reAttr.cap(1) + ":currentColor;");
        }
      }
    }
  }
  element.setAttribute("class", classname);
  return true;
}

bool cmd_colorToClass(QDomDocument &svg, const QColor &color, const QString &classname)
{
  QDomElement cur = svg.documentElement();
  QColor curColor;
  static const QStringList attributes = QStringList() << "fill" << "stroke" << "stop-color";
  while (!cur.isNull()) {
    foreach (QString name, attributes) {
      curColor = stringToColor(cur.attribute(name));
      if (curColor.isValid() && curColor == color) {
        if (!applyClassToElement(svg, cur, classname)) {
          return false;
        }
        cur.setAttribute(name, "currentColor");
      }
      QString style = cur.attribute("style");
      QRegExp reAttr("(?:^|;)\\s*" + name + "\\s*:\\s*([^;]+)\\s*(?:$|;)");
      if (reAttr.indexIn(style) >= 0 &&
          (curColor = stringToColor(reAttr.cap(1))) == color &&
          curColor.isValid()) {
        if (!applyClassToElement(svg, cur, classname)) {
          return false;
        }
        style = cur.attribute("style");
        reAttr.indexIn(style);
        style.replace(reAttr.pos(1), reAttr.cap(1).length(), "currentColor");
        cur.setAttribute("style", style);
      }
    }
    if (!cur.firstChildElement().isNull()) {
      cur = cur.firstChildElement();
    } else if (!cur.nextSiblingElement().isNull()) {
      cur = cur.nextSiblingElement();
    } else {
      do {
        cur = cur.parentNode().toElement();
      } while (cur.isElement() && cur.nextSiblingElement().isNull());
      cur = cur.nextSiblingElement();
    }
  }
  return true;
}

void cmd_reapply(QDomDocument &svg)
{
  QDomElement cur = svg.documentElement();
  QRegExp reColor("(^|;)\\s*color\\s*:\\s*[^;]+\\s*(?:$|;)");
  while (!cur.isNull()) {
    QString style = cur.attribute("style");
    cur.removeAttribute("color");
    if (reColor.indexIn(style) >= 0) {
      style.replace(reColor.pos(0), reColor.cap(0).length(), reColor.cap(1));
      cur.setAttribute("style", style);
    }
    if (!cur.firstChildElement().isNull()) {
      cur = cur.firstChildElement();
    } else if (!cur.nextSiblingElement().isNull()) {
      cur = cur.nextSiblingElement();
    } else {
      do {
        cur = cur.parentNode().toElement();
      } while (cur.isElement() && cur.nextSiblingElement().isNull());
      cur = cur.nextSiblingElement();
    }
  }
}

int main(int argc, char **argv)
{
  QList<Command> commandList;
  QString backup;

  int i;
  for (i = 1; i < argc; ++i) {
    QString arg(argv[i]);
    if (arg == "--") {
      ++i;
      break;
    } else if (arg == "--help" || arg == "-h") {
      displayHelp();
      return 0;
    } else if (arg == "--version" || arg == "-v") {
      displayVersion();
      return 0;
    } else if (arg == "--backup" || arg == "-b") {
      if (!checkCommand(arg, 1, i, argc)) {
        return TO_FEW_ARGUMENTS;
      }
      backup = argv[++i];
      continue;
    }
    Command command;
    if (arg == "--add-class" || arg == "-a") {
      if (!checkCommand(arg, 3, i, argc)) {
        return TO_FEW_ARGUMENTS;
      }
      command.cmd = addClass;
      command.arg.append(argv[++i]);
      command.arg.append(argv[++i]);
      command.arg.append(argv[++i]);
    } else if (arg == "--color-to-class" || arg == "-c") {
      if (!checkCommand(arg, 2, i, argc)) {
        return TO_FEW_ARGUMENTS;
      }
      command.cmd = colorToClass;
      command.arg.append(argv[++i]);
      command.arg.append(argv[++i]);
    } else if (arg == "--reapply" || arg == "-r") {
      command.cmd = reapply;
    } else {
      break;
    }
    commandList.append(command);
  }

  if (i >= argc) {
    if (commandList.size()) {
      std::cerr << "You need to provide at least one file for processing." << std::endl;
      return NO_FILES_PROVIDED;
    }
    displayHelp();
    return 0;
  } else if (!commandList.size()) {
    std::cerr << "You need to provide at least one command for processing." << std::endl;
    return NO_COMMANDS_PROVIDED;
  }

  for (; i < argc; ++i) {
    QFile file(argv[i]);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      std::cerr << "Could not open file '" << argv[i] << "' for reading." << std::endl;
      return OPENING_R_FAILED;
    }

    QDomDocument svg;
    if (!svg.setContent(&file)) {
      std::cerr << "Could not parse file '" << argv[i] << "'." << std::endl;
        return PARSING_FAILED;
    }
    file.close();

    for(int j = 0; j < commandList.size(); ++j) {
      switch(commandList[j].cmd) {
        case addClass:
          cmd_addClass(svg, commandList[j].arg[0],
                            commandList[j].arg[1],
                            stringToColor(commandList[j].arg[2]));
          break;
        case colorToClass:
          if (!cmd_colorToClass(svg, stringToColor(commandList[j].arg[0]),
                                commandList[j].arg[1])) {
            return APPLYING_CLASS_FAILED;
          }
          break;
        case reapply:
          cmd_reapply(svg);
          break;
      }
    }

    if(!backup.isEmpty()) {
      if (QFile::exists(file.fileName() + backup)) {
        QFile::remove(file.fileName() + backup);
      }
      if (!file.copy(file.fileName() + backup)) {
        std::wcerr << "Could not create a backup of file '" << file.fileName().toStdWString() << "'." << std::endl;
        return CREATING_BACKUP_FAILED;
      }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      std::cerr << "Could not open file '" << argv[i] << "' for writing." << std::endl;
      return OPENING_W_FAILED;
    }

    if (file.write(svg.toByteArray(2)) < 0) {
      std::cerr << "Could not write to file '" << argv[i] << "'." << std::endl;
      return WRITING_FAILED;
    }
  }

  return 0;
}
