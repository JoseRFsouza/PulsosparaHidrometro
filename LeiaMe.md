# 💧 HidroSense - Monitoramento Inteligente de Consumo de Água

O **HidroSense** é uma solução completa de IoT desenvolvida para monitorar o consumo de água em tempo real, focada na gestão de múltiplas unidades consumidoras. O projeto integra hardware simulado, processamento de dados em nuvem e um dashboard interativo para análise de consumo e geração de relatórios.

## 🔗 Ecossistema do Projeto

Para uma organização eficiente e profissional, o projeto foi dividido em dois repositórios independentes:

1.  **Dashboard (Este repositório):** Interface web desenvolvida em Next.js para visualização de métricas, gestão de dados e geração de relatórios em PDF.
2.  **Simulador de Hardware e Integração:** [JoseRFsouza/PulsosparaHidrometro](https://github.com/JoseRFsouza/PulsosparaHidrometro)
    *   Contém o firmware do ESP32 (C++), o esquema elétrico do Wokwi (`diagram.json`) e os fluxos do Node-RED para integração.

---

## 🚀 Como funciona o fluxo de dados?

O sistema opera através de uma arquitetura de 4 camadas principais:

1.  **Dispositivo IoT (Edge):** Um ESP32 (simulado via Wokwi) detecta pulsos de um sensor de fluxo virtual. Ele utiliza a biblioteca `PubSubClient` para enviar esses dados via protocolo **MQTT**.
2.  **Integração (Middleware):** O **Node-RED** atua como o "cérebro" da integração. Ele assina os tópicos MQTT, valida a integridade dos dados, adiciona *timestamps* e realiza o cálculo de consumo antes de enviar para a nuvem.
3.  **Banco de Dados (Cloud):** Os dados são armazenados de forma persistente e segura no **MongoDB Atlas**.
4.  **Interface (Frontend):** O dashboard em **Next.js** consome a API do MongoDB para exibir gráficos de consumo mensal e gerir as unidades consumidoras.

---

## ⚙️ Detalhamento Técnico

### 1. Inteligência com Node-RED
O Node-RED traduz as mensagens do mundo físico para o banco de dados:
*   **Tratamento de Dados:** Recebe JSONs como `{"leitura": 12.5, "unidade": 1}`, tratando-os para garantir que cheguem formatados ao banco.
*   **Flexibilidade:** O arquivo `flows.json` (no repositório de hardware) permite replicar toda a lógica de backend rapidamente.

### 2. Dashboard com Next.js
Projetado para performance e escalabilidade:
*   **API Routes:** Utiliza o diretório `app/api` para rotas internas seguras que se conectam ao MongoDB.
*   **Server Components:** Busca os dados no lado do servidor para garantir que as chaves de conexão (URI) nunca fiquem expostas no navegador.
*   **Relatórios:** Função integrada para exportação de dados de consumo em formato PDF para fins de auditoria.

### 3. Simulação com Wokwi
Utiliza a extensão Wokwi no VS Code para simular o ESP32. A simulação exige acesso à internet para que o hardware virtual consiga alcançar o Broker MQTT externo e alimentar o sistema.

---

## 🛠️ Tecnologias Utilizadas

*   **Frontend:** [Next.js](https://nextjs.org/) (App Router), Tailwind CSS, Lucide React.
*   **IoT/Hardware:** ESP32, Protocolo MQTT, Simulador [Wokwi](https://wokwi.com/).
*   **Integração:** [Node-RED](https://nodered.org/).
*   **Banco de Dados:** [MongoDB Atlas](https://www.mongodb.com/cloud/atlas).
*   **Deploy:** Vercel (Frontend).

---

## 🔧 Guia de Instalação e Configuração

Siga os passos abaixo para colocar o ecossistema completo para rodar:

### 1. Configuração do Banco de Dados (MongoDB Atlas)
*   Crie um cluster gratuito no [MongoDB Atlas](https://www.mongodb.com/cloud/atlas).
*   Crie uma base de dados chamada `HidroSense` e uma coleção chamada `leituras`.
*   Obtenha sua **Connection String** (URI) de conexão.

### 2. Configuração do Dashboard (Este repositório)
1.  Clone o projeto: `git clone https://github.com/JoseRFsouza/HidroSense.git`
2.  Instale as dependências: `npm install`
3.  Crie um arquivo `.env.local` na raiz do projeto e adicione sua URI:
    ```env
    MONGODB_URI=sua_string_de_conexao_do_atlas_aqui
    ```
4.  Rode o dashboard: `npm run dev` e acesse `http://localhost:3000`.

### 3. Configuração da Integração (Node-RED)
1.  Importe o arquivo `flows.json` (disponível na pasta `/node-red` do repositório de hardware).
2.  Configure o nó do MongoDB com sua string de conexão.
3.  Clique em **Deploy** para iniciar a escuta do Broker MQTT.

### 4. Execução da Simulação (Wokwi)
1.  Abra o repositório [PulsosparaHidrometro](https://github.com/JoseRFsouza/PulsosparaHidrometro) no VS Code.
2.  Com a extensão Wokwi instalada, abra o arquivo `.ino` e inicie o simulador (`F1` > `Wokwi: Start Simulator`).
3.  Acompanhe os dados chegando em tempo real no dashboard Next.js.

---

## 📝 Licença
Este projeto foi desenvolvido como parte de um **Projeto Integrador** acadêmico focado em IoT, Engenharia e Desenvolvimento Web.